/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/lock.h>

#define NUM_MAX_TASK 16

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
    TASK_ZOMBIE
} task_status_t;

typedef struct
{
    spin_lock_t lock;
    list_head block_queue;
} pcb_wait_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    uint64_t kernel_stack_base;
    uint64_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;
    
    /*wait queue for waitpid*/
    pcb_wait_t pcb_wait;
    
    spin_lock_t pcb_lock;
    
    /* whether killed ?*/
    int killed;
    
    /* whether recycleed ? */
    int recycle;
    
    /* whether forked ? */
    int fork;
    
    char name[32];
    
    uint64_t cpu_affinity;
    
    uint64_t running_core;

    uint64_t mem_sz;

    uint64_t pgdir;
} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running[2];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb;
extern pcb_t pid1_pcb;
extern const uint64_t pid0_stack;
extern const uint64_t pid1_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue, spin_lock_t *lock);
void do_unblock(list_node_t *);

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
extern void do_taskset(pid_t, uint64_t);
extern void do_pthread_create(pid_t*, void (*start_routine)(void*), void*);
extern pid_t do_fork();
/************************************************************/
extern int mycpu();

#endif
