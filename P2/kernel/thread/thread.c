#include<os/list.h>
#include <os/sched.h>
#include <os/mm.h>
#include <csr.h>
void thread_schedule(void);
void init_thread_pcb(uint64_t, int);
void init_thread_stack(uint64_t, uint64_t, uint64_t, int, pcb_t*);
static inline pcb_t* list_to_pcb(list_node_t*);
extern int pcb_count;
extern void ret_from_exception();

void thread_create(uint64_t func_entry, int thread_rank)
{
    init_thread_pcb(func_entry, thread_rank);
}

void thread_yield()
{
    thread_schedule();
}

void thread_schedule()
{
    pcb_t *pre_running;
    list_node_t *p;
    

    pre_running = current_running;
    
    pre_running->status = TASK_READY;
    add_to_list(&ready_queue, &(pre_running->ready_list));
    
    p = ready_queue.next;
    while (list_to_pcb(p)->pid != pre_running->pid){
        p = p->next;
    }
    current_running = list_to_pcb(p);
    leave_out_list(p); 
    current_running->status = TASK_RUNNING;
    process_id = current_running->pid;
    
    switch_to(pre_running, current_running);
}

void init_thread_pcb(uint64_t func_entry, int thread_rank)
{
    ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    ptr_t user_stack   = allocUserPage(1) + PAGE_SIZE;
    pcb_count++;
    pcb[pcb_count].pid    = current_running->pid;
    pcb[pcb_count].status = TASK_READY;
    pcb[pcb_count].user_sp = user_stack;
    add_to_list(&ready_queue, &pcb[pcb_count].ready_list);
    init_thread_stack(kernel_stack, user_stack, func_entry, thread_rank, &pcb[pcb_count]);
}

void init_thread_stack(uint64_t kernel_stack, uint64_t user_stack, uint64_t func_entry, int thread_rank, pcb_t *pcb)
{
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    uint64_t *trapframe = (uint64_t*)pt_regs;
    *(trapframe + 2)  = user_stack;
    *(trapframe + 4)  = pcb;
    *(trapframe + 10) = thread_rank;
    pt_regs->sepc     = func_entry;
    pt_regs->sstatus  = SR_SPIE; 
    
    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    uint64_t *ctx_sp = (uint64_t*)pt_switchto;
    pcb->kernel_sp = ctx_sp;
    *ctx_sp = ret_from_exception;
    *(ctx_sp + 1) = ctx_sp;
}

static inline pcb_t* list_to_pcb(list_node_t *p)
{
    unsigned offset = (unsigned)(&(((pcb_t*)0)->ready_list));
    return (pcb_t*)((void*)p - offset);
}