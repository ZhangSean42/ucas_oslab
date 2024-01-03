#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

static inline pcb_t* list_to_pcb(list_node_t*);
pcb_t* lock_to_pcb(list_node_t*);
pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .status = TASK_RUNNING
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    list_node_t *p = sleep_queue.next;
    while (p != &sleep_queue){
        if (get_timer() >= list_to_pcb(p)->wakeup_time){
            leave_out_list(p);
            pcb_t *pcb = list_to_pcb(p);
            p = p->next;
            pcb->status = TASK_READY;
            add_to_list(&ready_queue, &pcb->ready_list);
        }
        else{
          p = p->next;
        }
    }
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    pcb_t *pre_running;

    pre_running = current_running;
    if (pre_running->status == TASK_BLOCKED)
        goto choose_runnable;
    pre_running->status = TASK_READY;
    add_to_list(&ready_queue, &(pre_running->ready_list));
choose_runnable:
    current_running = list_to_pcb(ready_queue.next);
    leave_out_list(ready_queue.next); 
    current_running->status = TASK_RUNNING;
    process_id = current_running->pid;
    // TODO: [p2-task1] switch_to current_running
    switch_to(pre_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    do_block(&current_running->ready_list, &sleep_queue);
    current_running->wakeup_time = get_timer() + sleep_time;
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    add_to_list(queue, pcb_node);
    current_running->status = TASK_BLOCKED;
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    leave_out_list(pcb_node);
    pcb_t *p = lock_to_pcb(pcb_node);
    p->status = TASK_READY;
    add_to_list(&ready_queue, &p->ready_list);
}

static inline pcb_t* list_to_pcb(list_node_t *p)
{
    unsigned offset = (unsigned)(&(((pcb_t*)0)->ready_list));
    return (pcb_t*)((void*)p - offset);
}

pcb_t* lock_to_pcb(list_node_t *p)
{
    unsigned offset = (unsigned)(&(((pcb_t*)0)->lock_list));
    return (pcb_t*)((void*)p - offset);
}
