#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <csr.h>
#include<os/string.h>

extern void ret_from_exception();
extern mutex_lock_t mlocks[LOCK_NUM];

static inline pcb_t* list_to_pcb(list_node_t*);
pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid1_stack = pid0_stack + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack - sizeof(regs_context_t),
    .kernel_stack_base = 0x50500fe8,
    .user_sp = (ptr_t)pid0_stack,
    .status = TASK_RUNNING,
    .pcb_lock = {UNLOCKED},
    .name = "pid0",
    .killed = 0,
    .cpu_affinity = 1
};

pcb_t pid1_pcb = {
    .pid = 1,
    .kernel_sp = (ptr_t)pid1_stack - sizeof(regs_context_t),
    .kernel_stack_base = 0x50501fe8,
    .user_sp = (ptr_t)pid1_stack,
    .status = TASK_RUNNING,
    .pcb_lock = {UNLOCKED},
    .name = "pid1",
    .killed = 0,
    .cpu_affinity = 2
};

LIST_HEAD(ready_queue);
spin_lock_t ready_lock = {UNLOCKED};
LIST_HEAD(sleep_queue);
spin_lock_t sleep_lock = {UNLOCKED};
spin_lock_t kalloc_lock = {UNLOCKED};

/* current running task PCB */
pcb_t *volatile current_running[2];

/* global process id */
pid_t process_id = 1;

int global_pid = 2;

extern int task_num;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    
    list_node_t *p, *q, *r;
    spin_lock_acquire(&sleep_lock);
    r = sleep_queue.next;
    
    p = sleep_queue.next;
    while (p != &sleep_queue){
        q = p->next;
        if (get_timer() >= list_to_pcb(p)->wakeup_time){
            //printl("sub sleep_queue %s at %x\n", list_to_pcb(p)->name, p);
            do_unblock(p);
        }
        p = q;
    }
    spin_lock_release(&sleep_lock);
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    pcb_t * volatile pre_running;

    pre_running = current_running[get_current_cpu_id()];
    spin_lock_acquire(&ready_lock);
    if (pre_running->status == TASK_BLOCKED || pre_running->status == TASK_EXITED)
        goto choose_runnable;
    pre_running->status = TASK_READY;
    add_to_list(&ready_queue, &(pre_running->list));
choose_runnable:
    r = ready_queue.next;
    while (list_to_pcb(r)->cpu_affinity != 3 && list_to_pcb(r)->cpu_affinity != (get_current_cpu_id()+1))
        r = r->next;
    current_running[get_current_cpu_id()] = list_to_pcb(r);
    leave_out_list(r); 
    
    current_running[get_current_cpu_id()]->status = TASK_RUNNING;
    current_running[get_current_cpu_id()]->running_core = get_current_cpu_id();
    // TODO: [p2-task1] switch_to current_running
    switch_to(pre_running, current_running[get_current_cpu_id()]);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    spin_lock_acquire(&sleep_lock);
    current_running[get_current_cpu_id()]->wakeup_time = get_timer() + sleep_time;
    
    do_block(&current_running[get_current_cpu_id()]->list, &sleep_queue, &sleep_lock);
    spin_lock_release(&sleep_lock);
}

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    add_to_list(queue, pcb_node);
    current_running[get_current_cpu_id()]->status = TASK_BLOCKED;
    spin_lock_release(lock);
    
    do_scheduler();
    spin_lock_acquire(lock);
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    leave_out_list(pcb_node);
    pcb_t *p = list_to_pcb(pcb_node);
    p->status = TASK_READY;
    spin_lock_acquire(&ready_lock);
    add_to_list(&ready_queue, &p->list);
    spin_lock_release(&ready_lock);
}

static inline pcb_t* list_to_pcb(list_node_t *p)
{
    unsigned offset = (unsigned)(&(((pcb_t*)0)->list));
    return (pcb_t*)((void*)p - offset);
}
 
int do_waitpid(pid_t pid)
{
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if (pcb[i].pid == pid){
            spin_lock_acquire(&pcb[i].pcb_wait.lock);
            if (pcb[i].status != TASK_EXITED)
                do_block(&current_running[get_current_cpu_id()]->list, &pcb[i].pcb_wait.block_queue, &pcb[i].pcb_wait.lock);
            spin_lock_release(&pcb[i].pcb_wait.lock);
            return current_running[get_current_cpu_id()]->pid; 
        }
    }
    return 0;
}

void do_exit()
{
    spin_lock_acquire(&current_running[get_current_cpu_id()]->pcb_wait.lock);
    for (int i = 0; i < LOCK_NUM; i++){
        if (mlocks[i].owner == current_running[get_current_cpu_id()]->pid)
            do_mutex_lock_release(mlocks[i].key);
    }
    current_running[get_current_cpu_id()]->status = TASK_EXITED;
    while (current_running[get_current_cpu_id()]->pcb_wait.block_queue.next != &current_running[get_current_cpu_id()]->pcb_wait.block_queue)
        do_unblock(current_running[get_current_cpu_id()]->pcb_wait.block_queue.next);
    spin_lock_release(&current_running[get_current_cpu_id()]->pcb_wait.lock);
    bios_set_timer((uint64_t)(get_ticks() + TIMER_INTERVAL));
    do_scheduler();
    
}

int do_kill(pid_t pid)
{
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if (pcb[i].pid == pid){
            pcb[i].killed = 1;
            return 1;
        }
    }
    return 0;
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    int i, j;
    for (i = 0; i < NUM_MAX_TASK; i++){
        spin_lock_acquire(&pcb[i].pcb_lock);
        if (pcb[i].status == TASK_EXITED){
            pcb[i].killed = 0;
            pcb[i].pid = global_pid++;
            pcb[i].status = TASK_READY;
            pcb[i].cpu_affinity = current_running[get_current_cpu_id()]->cpu_affinity;
            spin_lock_release(&pcb[i].pcb_lock);
            break;
        }
        spin_lock_release(&pcb[i].pcb_lock);
    }
    if (i == NUM_MAX_TASK)
        return 0;
    
    spin_lock_acquire(&kalloc_lock);
    ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
    spin_lock_release(&kalloc_lock);
    
    uint64_t task_entry = load_task_img(name, task_num);
    
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pcb[i].kernel_stack_base = &pt_regs->sepc;
    uint64_t *trapframe = (uint64_t*)pt_regs;
    *(trapframe + 4) = &pcb[i];
    pt_regs->sepc    = task_entry;
    pt_regs->sstatus = SR_SPIE; 
    
    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    uint64_t *ctx_sp = (uint64_t*)pt_switchto;
    pcb[i].kernel_sp = ctx_sp;
    *ctx_sp = ret_from_exception;
    *(ctx_sp + 1) = ctx_sp;
    
    uint64_t* argv_base = user_stack - (argc + 1);
    uint64_t user_stack_now = argv_base;
    for (j = 0; j < argc; j++){
        int len = strlen(argv[j])+1;
        user_stack_now -= len;
        strcpy(user_stack_now, argv[j]);
        *((uint64_t*)(argv_base+j)) = user_stack_now;
    }
    *(trapframe + 2)  = user_stack_now & ~15;
    pt_regs->regs[10] = argc;
    pt_regs->regs[11] = argv_base;
    spin_lock_acquire(&ready_lock);
    add_to_list(&ready_queue, &pcb[i].list);
    spin_lock_release(&ready_lock);
    strcpy(pcb[i].name, name);
    
    return pcb[i].pid;
}

void do_process_show(char *buffer)
{
    buffer[0] = '\0';
    strcat(buffer, "\n[PROCESS TABLE]\n");

    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status != TASK_EXITED){
            strcat(buffer, "PID: ");
            char pid[10];
            itoa(pcb[i].pid, pid, 10, 10);
            strcat(buffer, pid);
            strcat(buffer, "  NAME: ");
            strcat(buffer, pcb[i].name);
            strcat(buffer, "  STATUS: ");
            if(pcb[i].status == TASK_BLOCKED){
                strcat(buffer, "BLOCKED");
            }
            else if(pcb[i].status == TASK_RUNNING){
                strcat(buffer, "RUNNING");
            }
            else if(pcb[i].status == TASK_READY){
                strcat(buffer, "READY");
            }
            strcat(buffer, "  MASK: 0x");
            char cpu_affinity[10];
            char running_core[10];
            itoa(pcb[i].cpu_affinity, cpu_affinity, 10, 10);
            strcat(buffer, cpu_affinity);
            if (pcb[i].status == TASK_RUNNING){
                strcat(buffer, "  Running on core: ");
                itoa(pcb[i].running_core, running_core, 10, 10);
                strcat(buffer, running_core);
            }
            strcat(buffer, "\n");
        }
    }
}

int do_getpid()
{
    return current_running[get_current_cpu_id()]->pid;
}

void switch_to_lock_release(void)
{
    spin_lock_release(&ready_lock);
}

void do_taskset(pid_t pid, uint64_t mask)
{
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            spin_lock_acquire(&pcb[i].pcb_lock);
            printl("mask pid%d\n", pcb[i].pid);
            pcb[i].cpu_affinity = mask;
            spin_lock_release(&pcb[i].pcb_lock);
        }
    }
}