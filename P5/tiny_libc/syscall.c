#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long return_value;
    asm volatile(
    "ecall\n\t"
    "mv %0, a0"
    :"=r"(return_value));

    return return_value;
}

void sys_yield(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    //call_jmptab(YIELD, 0, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    //call_jmptab(MOVE_CURSOR, x, y, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, x, y, 0, 0, 0);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    //call_jmptab(WRITE, buff, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, buff, 0, 0, 0, 0);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    //call_jmptab(REFLUSH, 0, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, 0, 0, 0, 0, 0);
}

void sys_clear(int line1, int line2)
{
    invoke_syscall(SYSCALL_CLEAR, line1, line2, 0, 0, 0);
}

void sys_printf(char *buff)
{
    invoke_syscall(SYSCALL_PRINTF, buff, 0, 0, 0, 0);
}

void sys_scroll(int line1, int line2)
{
    invoke_syscall(SYSCALL_SCROLL, line1, line2, 0, 0, 0);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    //call_jmptab(MUTEX_INIT, key, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    return (int)invoke_syscall(SYSCALL_LOCK_INIT, key, 0, 0, 0, 0);
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    //call_jmptab(MUTEX_ACQ, mutex_idx, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, 0, 0, 0, 0);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    //call_jmptab(MUTEX_RELEASE, mutex_idx, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, 0, 0, 0, 0);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE, 0, 0, 0, 0, 0);
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK, 0, 0, 0, 0, 0);
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, time, 0, 0, 0, 0);
}

void sys_thread_create(uint64_t func_entry, int thread_id)
{
    invoke_syscall(SYSCALL_THREAD_CREATE, func_entry, thread_id, 0, 0, 0);
}

void sys_thread_yield()
{
    invoke_syscall(SYSCALL_THREAD_YIELD, 0, 0, 0, 0, 0);
}
/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    invoke_syscall(SYSCALL_EXEC, name, argc, argv, 0, 0);
}
#endif

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    invoke_syscall(SYSCALL_KILL, pid, 0, 0, 0, 0);
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    invoke_syscall(SYSCALL_WAITPID, pid, 0, 0, 0, 0);
}

void sys_ps(char *buffer)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, buffer, 0, 0, 0, 0);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    invoke_syscall(SYSCALL_GETPID, 0, 0, 0, 0, 0);
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    invoke_syscall(SYSCALL_READCH, 0, 0, 0, 0, 0);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    invoke_syscall(SYSCALL_BARR_INIT, key, goal, 0, 0, 0);
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, 0, 0, 0, 0);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, 0, 0, 0, 0);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
    invoke_syscall(SYSCALL_SEMA_INIT, key, init, 0, 0, 0);
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
    invoke_syscall(SYSCALL_SEMA_UP, sema_idx, 0, 0, 0, 0);
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
    invoke_syscall(SYSCALL_SEMA_DOWN, sema_idx, 0, 0, 0, 0);
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
    invoke_syscall(SYSCALL_SEMA_DESTROY, sema_idx, 0, 0, 0, 0);
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    invoke_syscall(SYSCALL_MBOX_OPEN, name, 0, 0, 0, 0);
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, mbox_id, 0, 0, 0, 0);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    invoke_syscall(SYSCALL_MBOX_SEND, mbox_idx, msg, msg_length, 0, 0);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    invoke_syscall(SYSCALL_MBOX_RECV, mbox_idx, msg, msg_length, 0, 0);
}

void sys_taskset(pid_t pid, uint64_t mask)
{
    invoke_syscall(SYSCALL_TASKSET, pid, mask, 0, 0, 0);   
}

void* sys_shmpageget(int key)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpageget */
    invoke_syscall(SYSCALL_SHM_GET, key, 0, 0, 0, 0);
}

void sys_shmpagedt(void *addr)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpagedt */
    invoke_syscall(SYSCALL_SHM_DT, addr, 0, 0, 0, 0);
}

void sys_pthread_create(pthread_t *thread, void (*start_routine)(void*), void *arg)
{
    invoke_syscall(SYSCALL_PTHREAD_CREATE, thread, start_routine, arg, 0, 0);
}

void sys_fork()
{
    invoke_syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    return (int)invoke_syscall(SYSCALL_NET_SEND, txpacket, length, 0, 0, 0);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    return (int)invoke_syscall(SYSCALL_NET_RECV, rxbuffer, pkt_num, pkt_lens, 0, 0);
}

void sys_net_recv_stream(void *buffer, int *nbytes)
{
    invoke_syscall(SYSCALL_NET_RECV_STREAM, buffer, nbytes, 0, 0, 0);
}
/************************************************************/