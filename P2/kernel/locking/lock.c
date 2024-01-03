#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i=0; i < LOCK_NUM; i++){
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        spin_lock_init(&mlocks[i].lock);
        mlocks[i].key = -1;
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
    return lock->status == UNLOCKED ? 1 : 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (1){
        if (spin_lock_try_acquire(lock)){
            lock->status = LOCKED;
            return;
        }
        else{
            mutex_lock_t *p = (mutex_lock_t*)lock;
            do_block(&current_running->lock_list, &p->block_queue);
            do_scheduler();
        }
    }
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == key)
            return key;
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
            return;
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == mlock_idx){
            spin_lock_release(&mlocks[i].lock);
            //release
            while (mlocks[i].block_queue.next != &mlocks[i].block_queue)
                do_unblock(mlocks[i].block_queue.next);
            return;
        }
    }
}
