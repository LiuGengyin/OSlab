#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)//called in main.c
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++){
        mlocks[i].lock.status = UNLOCKED;
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].key = 0;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)//warning: 0 is an invalid key
{
    /* TODO: [p2-task2] initialize mutex lock */
    int i;
    for(i = 0; i < LOCK_NUM; i++){
        if(key == mlocks[i].key) break;
    }
    if(i != LOCK_NUM) return i;//the lock appointed is already initialized
    
    for(i = 0; i < LOCK_NUM; i++){
        if(mlocks[i].key == 0 /*|| mlocks[i].lock.status == UNLOCKED*/) break;
    }
    
    mlocks[i].key = key;
    return i;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if(mlocks[mlock_idx].lock.status == UNLOCKED){
       mlocks[mlock_idx].lock.status = LOCKED;
       return;
    }//the caller acquires the lock
    
    do_block(&(current_running->list), &(mlocks[mlock_idx].block_queue));
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    //mlocks[mlock_idx].lock.status = UNLOCKED;
    if(!do_unblock(&(mlocks[mlock_idx].block_queue))) mlocks[mlock_idx].lock.status = UNLOCKED;//mlocks[mlock_idx].key = 0;
    //the block_queue is empty means that the duty of this lock is accomplished. set key to 0 to enable another initialize.
}
