#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

static inline pcb_t* list_to_pcb(list_node_t *p)
{
    unsigned offset = (unsigned)(&(((pcb_t*)0)->list));
    return (pcb_t*)((void*)p - offset);
}

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i=0; i < LOCK_NUM; i++){
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].mutex_status = UNLOCKED;
        mlocks[i].key = -1;
        mlocks[i].owner = -1;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return atomic_swap_d(LOCKED, (lock_status_t*)lock);
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (spin_lock_try_acquire(lock) == LOCKED)
        ;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    atomic_swap_d(UNLOCKED, (lock_status_t*)lock);
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == key){
            return key;
        }
    }
    for (int i= 0; i < LOCK_NUM; i++){
        if (mlocks[i].key == -1){
            mlocks[i].key = key;
            return key;
        }
    }
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == mlock_idx){
            spin_lock_acquire(&mlocks[i].lock);
            while (mlocks[i].mutex_status == LOCKED)
                do_block(&current_running->list, &mlocks[i].block_queue, &mlocks[i].lock);
            mlocks[i].mutex_status = LOCKED;
            mlocks[i].owner = current_running->pid;
            spin_lock_release(&mlocks[i].lock);
            return;
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == mlock_idx){
            spin_lock_acquire(&mlocks[i].lock);
            mlocks[i].mutex_status = UNLOCKED;
            while (mlocks[i].block_queue.next != &mlocks[i].block_queue)
                do_unblock(mlocks[i].block_queue.next);
            mlocks[i].owner = -1;
            spin_lock_release(&mlocks[i].lock);
            return;
        }
    }
}