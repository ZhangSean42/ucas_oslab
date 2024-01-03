#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>

#include <printk.h>

mutex_lock_t mlocks[LOCK_NUM];
spin_lock_t mutex_init = {UNLOCKED};
semaphore_t sema[SEMAPHORE_NUM];
spin_lock_t sema_init = {UNLOCKED};
barrier_t bar[BARRIER_NUM];
spin_lock_t bar_init = {UNLOCKED};
mailbox_t mboxs[MBOX_NUM];
spin_lock_t mbox_init = {UNLOCKED};
spin_lock_t bois_lock = {UNLOCKED};

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
    return atomic_swap(LOCKED, (lock_status_t*)lock);
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
    atomic_swap(UNLOCKED, (lock_status_t*)lock);
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    spin_lock_acquire(&mutex_init);
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == key){
            spin_lock_release(&mutex_init);
            return key;
        }
    }
    for (int i= 0; i < LOCK_NUM; i++){
        if (mlocks[i].key == -1){
            mlocks[i].key = key;
            spin_lock_release(&mutex_init);
            return key;
        }
    }
    spin_lock_release(&mutex_init);
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    for (int i=0; i < LOCK_NUM; i++){
        if (mlocks[i].key == mlock_idx){
            spin_lock_acquire(&mlocks[i].lock);
            while (mlocks[i].mutex_status == LOCKED)
                do_block(&current_running[get_current_cpu_id()]->list, &mlocks[i].block_queue, &mlocks[i].lock);
            mlocks[i].mutex_status = LOCKED;
            mlocks[i].owner = current_running[get_current_cpu_id()]->pid;
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

void init_semaphores()
{
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        sema[i].block_queue.prev = &sema[i].block_queue;
        sema[i].block_queue.next = &sema[i].block_queue;
        spin_lock_init(&sema[i].lock);
        sema[i].key = -1;
    }
}

int do_semaphore_init(int key, int init)
{
    spin_lock_acquire(&sema_init);
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        if (sema[i].key == key){
            spin_lock_release(&sema_init);
            return key;
        }
    }
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        if (sema[i].key == -1){
            sema[i].key = key;
            sema[i].sema_num = init;
            spin_lock_release(&sema_init);
            return key;
        }
    }
    spin_lock_release(&sema_init);
    return -1;
}

void do_semaphore_up(int sema_idx)
{
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        if (sema[i].key == sema_idx){
            spin_lock_acquire(&sema[i].lock);
            sema[i].sema_num++;
            if (sema[i].sema_num <= 0){
                do_unblock(sema[i].block_queue.next);
            }
            spin_lock_release(&sema[i].lock);
            return;
        }
    }
}

void do_semaphore_down(int sema_idx)
{
    for (int i=0; i < SEMAPHORE_NUM; i++){
        if (sema[i].key == sema_idx){
            spin_lock_acquire(&sema[i].lock);
            sema[i].sema_num--;
            if (sema[i].sema_num < 0)
                do_block(&current_running[get_current_cpu_id()]->list, &sema[i].block_queue, &sema[i].lock);
            spin_lock_release(&sema[i].lock);
            return;
        }
    }
}

void do_semaphore_destroy(int sema_idx)
{
    spin_lock_acquire(&sema_init);
    for (int i = 0; i < SEMAPHORE_NUM; i++){
        if (sema[i].key == sema_idx){
            sema[i].block_queue.prev = &sema[i].block_queue;
            sema[i].block_queue.next = &sema[i].block_queue;
            spin_lock_init(&sema[i].lock);
            sema[i].key = -1;
            spin_lock_release(&sema_init);
            return;
        }
    }
    spin_lock_release(&sema_init);
}

void init_barriers()
{
    for (int i = 0; i < BARRIER_NUM; i++){
        bar[i].block_queue.prev = &bar[i].block_queue;
        bar[i].block_queue.next = &bar[i].block_queue;
        spin_lock_init(&bar[i].lock);
        bar[i].key = -1;
        bar[i].bar_num = 0;
    }
}

int do_barrier_init(int key, int goal)
{
    spin_lock_acquire(&bar_init);
    for (int i = 0; i < BARRIER_NUM; i++){
        if (bar[i].key == key){
            spin_lock_release(&bar_init);
            return key;
        }
    }
    for (int i = 0; i < BARRIER_NUM; i++){
        if (bar[i].key == -1){
            bar[i].key = key;
            bar[i].goal = goal;
            spin_lock_release(&bar_init);
            return key;
        }
    }
    spin_lock_release(&bar_init);
    return -1;
}

void do_barrier_wait(int bar_idx)
{
    for (int i = 0; i < BARRIER_NUM; i++){
        if (bar[i].key == bar_idx){
            spin_lock_acquire(&bar[i].lock);
            bar[i].bar_num++;
            if (bar[i].bar_num < bar[i].goal){
                do_block(&current_running[get_current_cpu_id()]->list, &bar[i].block_queue, &bar[i].lock);
            }
            else if (bar[i].bar_num == bar[i].goal){
                while (bar[i].block_queue.next != &bar[i].block_queue)
                    do_unblock(bar[i].block_queue.next);
                bar[i].bar_num = 0;
            }
            spin_lock_release(&bar[i].lock);
            
        }
    }
}

void do_barrier_destroy(int bar_idx)
{
    spin_lock_acquire(&bar_init);
    for (int i = 0; i < BARRIER_NUM; i++){
        if (bar[i].key == bar_idx){
            bar[i].block_queue.prev = &bar[i].block_queue;
            bar[i].block_queue.next = &bar[i].block_queue;
            spin_lock_init(&bar[i].lock);
            bar[i].key = -1;
            bar[i].bar_num = 0;
            spin_lock_release(&bar_init);
            return;
        }
    }
    spin_lock_release(&bar_init);
}

void init_mbox()
{
    for (int i = 0; i < MBOX_NUM; i++){
        mboxs[i].usnr = 0;
        mboxs[i].key[0] = '\0';
    }
}

int do_mbox_open(char *name)
{
    spin_lock_acquire(&mbox_init);
    for (int i = 0; i < MBOX_NUM; i++){
        if (!strcmp(mboxs[i].key, name)){
            mboxs[i].usnr++;
            spin_lock_release(&mbox_init);
            return i;
        }
    }
    for (int i = 0; i < MBOX_NUM; i++){
        if (mboxs[i].key[0] == '\0'){
            strcpy(mboxs[i].key, name);
            mboxs[i].usnr++;
            mboxs[i].lock.status = UNLOCKED;
            mboxs[i].buffer_head = 0;
            mboxs[i].buffer_tail = 0;
            mboxs[i].send_block_queue.prev = &mboxs[i].send_block_queue;
            mboxs[i].send_block_queue.next = &mboxs[i].send_block_queue;
            mboxs[i].recv_block_queue.prev = &mboxs[i].recv_block_queue;
            mboxs[i].recv_block_queue.next = &mboxs[i].recv_block_queue;
            spin_lock_release(&mbox_init);
            return i;
        }
    }
    spin_lock_release(&mbox_init);
    return -1;
}

int mbox_buffer_full(int mbox_idx, int len)
{
    return (((mboxs[mbox_idx].buffer_tail-mboxs[mbox_idx].buffer_head+MAX_MBOX_LENGTH) % MAX_MBOX_LENGTH) + len >= MAX_MBOX_LENGTH);
}
int mbox_buffer_empty(int mbox_idx, int len)
{
    return (((mboxs[mbox_idx].buffer_tail-mboxs[mbox_idx].buffer_head+MAX_MBOX_LENGTH) % MAX_MBOX_LENGTH) < len);
}
void mbox_buffer_add(int mbox_idx, char ch)
{
    mboxs[mbox_idx].mbox_buf[mboxs[mbox_idx].buffer_tail] = ch;
    mboxs[mbox_idx].buffer_tail = (mboxs[mbox_idx].buffer_tail + 1) % MAX_MBOX_LENGTH;
}
char mbox_buffer_sub(int mbox_idx)
{
    char ret = mboxs[mbox_idx].mbox_buf[mboxs[mbox_idx].buffer_head];
    mboxs[mbox_idx].buffer_head = (mboxs[mbox_idx].buffer_head + 1) % MAX_MBOX_LENGTH;
    return ret;
}

int do_mbox_send(int mbox_idx, char *msg, int msg_length)
{
    spin_lock_acquire(&mboxs[mbox_idx].lock);
    while (mbox_buffer_full(mbox_idx, msg_length)){
        do_block(&current_running[get_current_cpu_id()]->list, &mboxs[mbox_idx].send_block_queue, &mboxs[mbox_idx].lock);
    }
    while (msg_length--){
        mbox_buffer_add(mbox_idx, *msg);
        msg++;
    }
    while (mboxs[mbox_idx].recv_block_queue.next != &mboxs[mbox_idx].recv_block_queue)
        do_unblock(mboxs[mbox_idx].recv_block_queue.next);
    spin_lock_release(&mboxs[mbox_idx].lock);
}

int do_mbox_recv(int mbox_idx, char *msg, int msg_length)
{
    spin_lock_acquire(&mboxs[mbox_idx].lock);
    while (mbox_buffer_empty(mbox_idx, msg_length)){
        do_block(&current_running[get_current_cpu_id()]->list, &mboxs[mbox_idx].recv_block_queue, &mboxs[mbox_idx].lock);
    }
    while (msg_length--){
        char ret = mbox_buffer_sub(mbox_idx);
        *msg++ = ret;
    }
    while (mboxs[mbox_idx].send_block_queue.next != &mboxs[mbox_idx].send_block_queue)
        do_unblock(mboxs[mbox_idx].send_block_queue.next);
    spin_lock_release(&mboxs[mbox_idx].lock);
}

void do_mbox_close(int mbox_idx)
{
    spin_lock_acquire(&mbox_init);
    mboxs[mbox_idx].usnr--;
    if (!mboxs[mbox_idx].usnr){
        mboxs[mbox_idx].key[0] = '\0';
    }
    spin_lock_release(&mbox_init);
}

