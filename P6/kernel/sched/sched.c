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
#include <os/string.h>
#include <os/net.h>

extern int get_current_cpu_id();
extern void ret_from_exception();
extern void RSD_and_ACK();
extern mutex_lock_t mlocks[LOCK_NUM];
extern run_t free_list;
extern run_t paging_list;
static inline pcb_t* list_to_pcb(list_node_t*);
pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
const ptr_t pid1_stack = pid0_stack + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (uint64_t)pid0_stack - sizeof(regs_context_t),
    .kernel_stack_base = 0xffffffc052000fe8,
    .user_sp = (uint64_t)pid0_stack,
    .status = TASK_RUNNING,
    .pcb_lock = {UNLOCKED},
    .name = "pid0",
    .killed = 0,
    .cpu_affinity = 1,
    .pgdir = PGDIR_PA + 0xffffffc000000000lu
};

pcb_t pid1_pcb = {
    .pid = 1,
    .kernel_sp = (uint64_t)pid1_stack - sizeof(regs_context_t),
    .kernel_stack_base = 0xffffffc052001fe8,
    .user_sp = (uint64_t)pid1_stack,
    .status = TASK_RUNNING,
    .pcb_lock = {UNLOCKED},
    .name = "pid1",
    .killed = 0,
    .cpu_affinity = 2,
    .pgdir = PGDIR_PA + 0xffffffc000000000lu
};

LIST_HEAD(ready_queue);
spin_lock_t ready_lock = {UNLOCKED};
LIST_HEAD(sleep_queue);
spin_lock_t sleep_lock = {UNLOCKED};
spin_lock_t kalloc_lock = {UNLOCKED};
uint64_t RSD_resend_time, ACK_resend_time;
int first_resend = 1;

/* current running task PCB */
pcb_t *volatile current_running[2];

/* global process id */
pid_t process_id = 1;

int global_pid = 2;
extern int task_num;

void do_scheduler(void)
{
    /*if (first_resend){
        for (int i = 0; i < NUM_MAX_TASK; i++){
            if (pcb[i].status != TASK_EXITED && strcmp(pcb[i].name, "recv_stream") == 0){
                RSD_resend_time = get_timer() + 8;
                ACK_resend_time = get_timer() + 8;
                first_resend = 0;
            }
        }
    }
    else{ 
        if (get_timer() >= RSD_resend_time){
            do_RSD();
            RSD_resend_time += 8;
        }
        if (get_timer() >= ACK_resend_time){
            do_ACK();
            ACK_resend_time += 8;
        }
    }*/
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    
    list_node_t *p, *q, *r;
    spin_lock_acquire(&sleep_lock);
    
    p = sleep_queue.next;
    while (p != &sleep_queue){
        q = p->next;
        if (get_timer() >= list_to_pcb(p)->wakeup_time){
            do_unblock(p);
        }
        p = q;
    }
    spin_lock_release(&sleep_lock);
    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/
    // recycle resouces from exited process if needed
    for (int i = 0; i < NUM_MAX_TASK; i++){
        if (pcb[i].status == TASK_EXITED && pcb[i].recycle == 1 && current_running[get_current_cpu_id()]->pid == pcb[0].pid){
            pcb[i].recycle = 0;
            uvmdealloc_all((PTE*)pcb[i].pgdir);
            free_walk((PTE*)pcb[i].pgdir ,0);
            freePage(pcb[i].kernel_stack_base, &paging_list);
        }
    }
    // TODO: [p2-task1] Modify the current_running pointer.
    pcb_t * volatile pre_running;

    pre_running = current_running[get_current_cpu_id()];
    spin_lock_acquire(&ready_lock);
    if (pre_running->status == TASK_BLOCKED || pre_running->status == TASK_EXITED || pre_running->status == TASK_ZOMBIE)
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
    set_satp(SATP_MODE_SV39, current_running[get_current_cpu_id()]->pid, kva2pa(current_running[get_current_cpu_id()]->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    local_flush_icache_all();
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
            if (pcb[i].status != TASK_ZOMBIE && pcb[i].status != TASK_EXITED){
                do_block(&current_running[get_current_cpu_id()]->list, &pcb[i].pcb_wait.block_queue, &pcb[i].pcb_wait.lock);
            }
            spin_lock_release(&pcb[i].pcb_wait.lock);
            if (pcb[i].status == TASK_ZOMBIE){
                pcb[i].recycle = 0;
                pcb[i].status = TASK_EXITED;
                freePage(pcb[i].kernel_stack_base, &paging_list);
                if (pcb[i].fork == 1){
                    free_walk((PTE*)pcb[i].pgdir ,0);
                }
                if (pcb[i].fork == 2){
                    unmap_normal_page(USER_STACK_ADDR, (PTE*)pcb[i].pgdir, 1);
                }
            }
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
    if (current_running[get_current_cpu_id()]->fork != 0){
        current_running[get_current_cpu_id()]->status = TASK_ZOMBIE;
    }
    else{
        current_running[get_current_cpu_id()]->status = TASK_EXITED;
    }
    while (current_running[get_current_cpu_id()]->pcb_wait.block_queue.next != &current_running[get_current_cpu_id()]->pcb_wait.block_queue)
        do_unblock(current_running[get_current_cpu_id()]->pcb_wait.block_queue.next);
    spin_lock_release(&current_running[get_current_cpu_id()]->pcb_wait.lock);
    current_running[get_current_cpu_id()]->recycle = 1;
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
    char buff[32];
    for (i = 0; i < NUM_MAX_TASK; i++){
        spin_lock_acquire(&pcb[i].pcb_lock);
        if (pcb[i].status == TASK_EXITED){
            pcb[i].killed = 0;
            pcb[i].pid = global_pid++;
            pcb[i].status = TASK_READY;
            pcb[i].cpu_affinity = current_running[get_current_cpu_id()]->cpu_affinity;
            pcb[i].fork = 0;
            strcpy(pcb[i].name, name);
            pcb[i].cwd = pcb[0].cwd;
            strcpy(pcb[i].cwd_path, pcb[0].cwd_path);
            spin_lock_release(&pcb[i].pcb_lock);
            break;
        }
        spin_lock_release(&pcb[i].pcb_lock);
    }
    if (i == NUM_MAX_TASK)
        return 0;
    
    spin_lock_acquire(&kalloc_lock);
    uint64_t kernel_stack = (uint64_t)kalloc(&paging_list) + PAGE_SIZE;  //allocate user's kernel stack from paging_list to avoid page-fault-exception in supervisor mode
    uint64_t user_stack   = (uint64_t)kalloc(&free_list) + PAGE_SIZE;
    spin_lock_release(&kalloc_lock);
    
    pcb[i].mem_sz = load_task_img(name, task_num, i);
    map_normal_page(USER_STACK_ADDR-PAGE_SIZE, user_stack-PAGE_SIZE, (PTE*)pcb[i].pgdir, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    register_run(USER_STACK_ADDR-PAGE_SIZE, user_stack-PAGE_SIZE, pcb[i].pgdir, 1);
    
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pcb[i].kernel_stack_base = kernel_stack - PAGE_SIZE;
    uint64_t *trapframe = (uint64_t*)pt_regs;
    *(trapframe + 4) = &pcb[i];
    pt_regs->sepc    = USR_START;
    pt_regs->sstatus = SR_SPIE | SR_SUM; 
    
    switchto_context_t *pt_switchto = (switchto_context_t *)((uint64_t)pt_regs - sizeof(switchto_context_t));
    uint64_t *ctx_sp = (uint64_t*)pt_switchto;
    pcb[i].kernel_sp = ctx_sp;
    *ctx_sp = ret_from_exception;
    *(ctx_sp + 1) = ctx_sp;
    
    uint64_t* argv_base = (uint64_t*)user_stack - (argc + 1);
    uint64_t user_stack_now = (uint64_t)argv_base;
    for (j = 0; j < argc; j++){
        uint64_t *str_ptr_kvaddr = (uint64_t*)fetchaddr((uint64_t)&argv[j], (PTE*)current_running[get_current_cpu_id()]->pgdir);
        uint64_t str_vaddr = *str_ptr_kvaddr;
        int len = 0;
        char *str_kvaddr;
        do{
            str_kvaddr = (char*)fetchaddr(str_vaddr, (PTE*)current_running[get_current_cpu_id()]->pgdir);
            str_vaddr++;
            buff[len++] = *str_kvaddr;
        }while(*str_kvaddr != '\0');
        user_stack_now -= len;
        strcpy(user_stack_now, buff);
        *((uint64_t*)(argv_base+j)) = USER_STACK_ADDR -(user_stack - user_stack_now);
    }
    *(trapframe + 2)  = (USER_STACK_ADDR -(user_stack - user_stack_now)) & ~15;
    pt_regs->regs[10] = argc;
    pt_regs->regs[11] = USER_STACK_ADDR - (user_stack - (uint64_t)argv_base);
    spin_lock_acquire(&ready_lock);
    add_to_list(&ready_queue, &pcb[i].list);
    spin_lock_release(&ready_lock);
    
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
            else if (pcb[i].status == TASK_ZOMBIE){
                strcat(buffer, "ZOMBIE");
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
            pcb[i].cpu_affinity = mask;
            spin_lock_release(&pcb[i].pcb_lock);
        }
    }
}

void do_pthread_create(pid_t *thread, void (*start_routine)(void*), void *arg)
{
    int i, j;
    char buff[32];
    for (i = 0; i < NUM_MAX_TASK; i++){
        spin_lock_acquire(&pcb[i].pcb_lock);
        if (pcb[i].status == TASK_EXITED){
            pcb[i].killed = 0;
            pcb[i].pid = global_pid++;
            *thread = pcb[i].pid;
            pcb[i].pgdir = current_running[get_current_cpu_id()]->pgdir;
            pcb[i].status = TASK_READY;
            pcb[i].cpu_affinity = current_running[get_current_cpu_id()]->cpu_affinity;
            pcb[i].fork = 2;
            spin_lock_release(&pcb[i].pcb_lock);
            break;
        }
        spin_lock_release(&pcb[i].pcb_lock);
    }
    if (i == NUM_MAX_TASK)
        return 0;
    
    spin_lock_acquire(&kalloc_lock);
    uint64_t kernel_stack = (uint64_t)kalloc(&paging_list) + PAGE_SIZE;
    uint64_t user_stack   = (uint64_t)kalloc(&free_list) + PAGE_SIZE;
    spin_lock_release(&kalloc_lock);
    
    map_normal_page(USER_STACK_ADDR, user_stack-PAGE_SIZE, (PTE*)pcb[i].pgdir, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    register_run(USER_STACK_ADDR, user_stack-PAGE_SIZE, pcb[i].pgdir, 0);
    
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pcb[i].kernel_stack_base = kernel_stack - PAGE_SIZE;
    uint64_t *trapframe = (uint64_t*)pt_regs;
    *(trapframe + 2)  = USER_STACK_ADDR + PAGE_SIZE;
    *(trapframe + 4) = &pcb[i];
    pt_regs->sepc    = start_routine;
    pt_regs->sstatus = SR_SPIE | SR_SUM; 
    
    switchto_context_t *pt_switchto = (switchto_context_t *)((uint64_t)pt_regs - sizeof(switchto_context_t));
    uint64_t *ctx_sp = (uint64_t*)pt_switchto;
    pcb[i].kernel_sp = ctx_sp;
    *ctx_sp = ret_from_exception;
    *(ctx_sp + 1) = ctx_sp;
    
    pt_regs->regs[10] = arg;
    spin_lock_acquire(&ready_lock);
    add_to_list(&ready_queue, &pcb[i].list);
    spin_lock_release(&ready_lock);
}

pid_t do_fork()
{
    int i, j;
    char buff[32];
    for (i = 0; i < NUM_MAX_TASK; i++){
        spin_lock_acquire(&pcb[i].pcb_lock);
        if (pcb[i].status == TASK_EXITED){
            pcb[i].killed = 0;
            pcb[i].pid = global_pid++;
            pcb[i].status = TASK_READY;
            pcb[i].cpu_affinity = current_running[get_current_cpu_id()]->cpu_affinity;
            pcb[i].fork = 1;
            spin_lock_release(&pcb[i].pcb_lock);
            break;
        }
        spin_lock_release(&pcb[i].pcb_lock);
    }
    if (i == NUM_MAX_TASK)
        return 0;
    
    spin_lock_acquire(&kalloc_lock);
    uint64_t kernel_stack = (uint64_t)kalloc(&paging_list) + PAGE_SIZE;
    spin_lock_release(&kalloc_lock);
    pcb[i].kernel_stack_base = kernel_stack - PAGE_SIZE;
    
    regs_context_t *father_pt_regs = current_running[get_current_cpu_id()]->kernel_sp;
    regs_context_t *son_pt_regs    = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    memcpy((uint8_t*)son_pt_regs, (uint8_t*)father_pt_regs, sizeof(regs_context_t));
    son_pt_regs->regs[4]  = &pcb[i];  
    son_pt_regs->regs[10] = 0;
    
    switchto_context_t *son_pt_switchto = (switchto_context_t *)((uint64_t)son_pt_regs - sizeof(switchto_context_t));
    pcb[i].kernel_sp = (uint64_t)son_pt_switchto;
    son_pt_switchto->regs[0] = ret_from_exception;
    son_pt_switchto->regs[1] = (uint64_t)son_pt_switchto;
    
    PTE* son_pgdir = (PTE*)kalloc(&paging_list);
    pcb[i].pgdir = (uint64_t)son_pgdir;
    share_pgtable(son_pgdir, (PTE*)pa2kva(PGDIR_PA));
    
    PTE* father_pgdir = (PTE*)current_running[get_current_cpu_id()]->pgdir;
    for (uint64_t i = 0; i < (NUM_PTE_ENTRY >> 1); i++){
        if (father_pgdir[i] & _PAGE_PRESENT){
            PTE* son_pmd = (PTE*)kalloc(&paging_list);
            set_pfn(&son_pgdir[i], kva2pa((uint64_t)son_pmd) >> NORMAL_PAGE_SHIFT);
            set_attribute(&son_pgdir[i], _PAGE_PRESENT);
            PTE* father_pmd = (PTE*)pa2kva(get_pfn(father_pgdir[i]));
            for (uint64_t j = 0; j < NUM_PTE_ENTRY; j++){
                if (father_pmd[j] & _PAGE_PRESENT){
                    PTE* son_pgt = (PTE*)kalloc(&paging_list);
                    set_pfn(&son_pmd[j], kva2pa((uint64_t)son_pgt) >> NORMAL_PAGE_SHIFT);
                    set_attribute(&son_pmd[j], _PAGE_PRESENT);
                    PTE* father_pgt = (PTE*)pa2kva(get_pfn(father_pmd[j]));
                    for (uint64_t k = 0; k < NUM_PTE_ENTRY; k++){
                        if (father_pgt[k] & _PAGE_PRESENT){
                            memcpy((uint8_t*)&son_pgt[k], (uint8_t*)&father_pgt[k], sizeof(uint64_t));
                            uint64_t perm = get_attribute(son_pgt[k]);
                            set_attribute(&son_pgt[k], perm & ~_PAGE_WRITE);
                        }
                        else{
                            continue;
                        }
                    }
                }
                else{
                    continue;
                }
            }
        }
        else{
            continue;
        }
    }
    
    spin_lock_acquire(&ready_lock);
    add_to_list(&ready_queue, &pcb[i].list);
    spin_lock_release(&ready_lock);
    
    return pcb[i].pid;
}