#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t mbarriers[BARRIER_NUM];
condition_t mconditions[CONDITION_NUM];
mailbox_t mmboxs[MBOX_NUM];

/***************************************************lock*******************************************/

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
       for(int i = 0; i < 16; i++){
           if(current_running->lockid[i] == -1){
               current_running->lockid[i] = mlock_idx;
               break;
           }
       }
       return;
    }//the caller acquires the lock
    
    do_block(&(current_running->list), &(mlocks[mlock_idx].block_queue));
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    //mlocks[mlock_idx].lock.status = UNLOCKED;
    if(!do_unblock(&(mlocks[mlock_idx].block_queue))) mlocks[mlock_idx].lock.status = UNLOCKED;
    for(int i = 0; i < 16; i++){
        if(current_running->lockid[i] == mlock_idx){
            current_running->lockid[i] = -1;
            break;
        }
    }
}

void release_for_kill(int mlock_idx){
    if(!do_unblock(&(mlocks[mlock_idx].block_queue))) mlocks[mlock_idx].lock.status = UNLOCKED;
} //考虑如下情况：被kill的进程曾申请两把锁，但在被kill的时刻已经释放了其中一把。如果我们在正常的release函数中不清空此pcb中那个lockid，则在kill的扫描中也将再次释放那个已经被释放的锁。然而这把锁可能已经作他用。所以，正常的release函数一定要清空对应的lockid。但是清空用的肯定是current_running，而kill是不能用current_running的。所以，为kill单设一个释放函数，不清空任何的lockid，而在do_kill中将被kill的pcb所有lockid清空。

void do_mutex_lock_destroy(int mlock_idx){
    mlocks[mlock_idx].key = 0;
    mlocks[mlock_idx].block_queue.next = mlocks[mlock_idx].block_queue.prev = &(mlocks[mlock_idx].block_queue);
    mlocks[mlock_idx].lock.status = UNLOCKED;
}

/************************************************************barrier***************************************/

void init_barriers(){
    for(int i = 0; i < BARRIER_NUM; i++){
        mbarriers[i].waiting = 0;
        mbarriers[i].limit = 0;
        mbarriers[i].barrier_queue.next = mbarriers[i].barrier_queue.prev = &(mbarriers[i].barrier_queue);
        mbarriers[i].key = 0;
    }
}

int do_barrier_init(int key, int goal){
    int i;
    for(i = 0; i < BARRIER_NUM; i++)
        if(mbarriers[i].key == key)
            break;
    if(i != BARRIER_NUM){
        mbarriers[i].limit = goal;
        return i;
    }
    
    for(i = 0; i < BARRIER_NUM; i++){
        if(mbarriers[i].key == 0) break;
    }
    mbarriers[i].key = key;
    mbarriers[i].limit = goal;
    return i;
}

void do_barrier_wait(int bar_idx){
    if(mbarriers[bar_idx].waiting + 1 < mbarriers[bar_idx].limit){
        mbarriers[bar_idx].waiting++;
        do_block(&(current_running->list), &(mbarriers[bar_idx].barrier_queue));
    }
    else{
        while(mbarriers[bar_idx].barrier_queue.next != &(mbarriers[bar_idx].barrier_queue)){
            //Enqueue(Dequeue(&(mbarriers[bar_idx].barrier_queue)), &ready_queue);
            do_unblock(&(mbarriers[bar_idx].barrier_queue));
        }
        mbarriers[bar_idx].waiting = 0;
    }
}

void do_barrier_destroy(int bar_idx){
    mbarriers[bar_idx].waiting = 0;
    mbarriers[bar_idx].limit = 0;
    mbarriers[bar_idx].key = 0;
}

/**********************************************************condition************************************/

void init_conditions(){
    for(int i = 0; i < CONDITION_NUM; i++){
        mconditions[i].waiting = 0;
        mconditions[i].cond_queue.next = mconditions[i].cond_queue.prev = &(mconditions[i].cond_queue);
        mconditions[i].key = 0;
    }
}

int do_condition_init(int key){
    int i;
    for(i = 0; i < CONDITION_NUM; i++){
        if(key == mconditions[i].key) break;
    }
    if(i != CONDITION_NUM)
        return i;
    
    for(i = 0; i < CONDITION_NUM; i++){
        if(mconditions[i].key == 0)
            break;
    }
    mconditions[i].key = key;
    mconditions[i].waiting = 0;
    return i;
}

void do_condition_wait(int cond_idx, int mutex_idx){
    mconditions[cond_idx].waiting++;
    do_mutex_lock_release(mutex_idx);
    do_block(&(current_running->list), &(mconditions[cond_idx].cond_queue));
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx){
    if(mconditions[cond_idx].waiting > 0){
        do_unblock(&(mconditions[cond_idx].cond_queue));
        mconditions[cond_idx].waiting--;
    }
}

void do_condition_broadcast(int cond_idx){
    if(mconditions[cond_idx].waiting > 0){
        while(mconditions[cond_idx].cond_queue.next != &(mconditions[cond_idx].cond_queue)){
            do_unblock(&(mconditions[cond_idx].cond_queue));
        }
        mconditions[cond_idx].waiting = 0;
    }
}

void do_condition_destroy(int cond_idx){
    mconditions[cond_idx].waiting = 0;
    mconditions[cond_idx].key = 0;
}

/*******************************************************mailbox****************************************/

void init_mbox(){
    for(int i = 0; i < MBOX_NUM; i++){
        mmboxs[i].name[0] = '\0';
        mmboxs[i].used = 0;
        mmboxs[i].refer = 0;
        mmboxs[i].buffer[0] = '\0';
        
        mmboxs[i].lk.lock.status = UNLOCKED;
        mmboxs[i].lk.block_queue.next = mmboxs[i].lk.block_queue.prev = &(mmboxs[i].lk.block_queue);
        
        mmboxs[i].empty.waiting = 0;
        mmboxs[i].empty.cond_queue.next = mmboxs[i].empty.cond_queue.prev = &(mmboxs[i].empty.cond_queue);
        mmboxs[i].full.waiting = 0;
        mmboxs[i].full.cond_queue.next = mmboxs[i].full.cond_queue.prev = &(mmboxs[i].full.cond_queue);
    }
}

int do_mbox_open(char* name){
    int i;
    for(i = 0; i < MBOX_NUM; i++)
        if(strcmp(name, mmboxs[i].name) == 0)
            break;
    
    if(i == MBOX_NUM){
        for(i = 0; i < MBOX_NUM; i++)
            if(mmboxs[i].name[0] == '\0')
                break;
        
        strcpy(mmboxs[i].name, name);
        mmboxs[i].refer++;
        return i;
    }
    
    else{
        mmboxs[i].refer++;
        return i;
    }
}

void do_mbox_close(int mbox_idx){
    mmboxs[mbox_idx].refer--;
    if(mmboxs[mbox_idx].refer <= 0){
    //reset the mailbox
        mmboxs[mbox_idx].refer = 0;
        mmboxs[mbox_idx].name[0] = '\0';
        mmboxs[mbox_idx].used = 0;
        mmboxs[mbox_idx].buffer[0] = '\0';
    //is it necessary to reset lock and conditions?
    }
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length){
    int times = 0;
    //lk.acquire
    if(mmboxs[mbox_idx].lk.lock.status == UNLOCKED){
        mmboxs[mbox_idx].lk.lock.status = LOCKED;
    }
    else
        do_block(&(current_running->list), &(mmboxs[mbox_idx].lk.block_queue));
    
    //critical area
    while((mmboxs[mbox_idx].used + msg_length) > (MAX_MBOX_LENGTH - 1)){
        //full.wait
        mmboxs[mbox_idx].full.waiting++;
        
        if(!do_unblock(&(mmboxs[mbox_idx].lk.block_queue))) mmboxs[mbox_idx].lk.lock.status = UNLOCKED;
        
        do_block(&(current_running->list), &(mmboxs[mbox_idx].full.cond_queue));
        
        if(mmboxs[mbox_idx].lk.lock.status == UNLOCKED){
            mmboxs[mbox_idx].lk.lock.status = LOCKED;
        }
        else
            do_block(&(current_running->list), &(mmboxs[mbox_idx].lk.block_queue));
            
        times++;
    }
    
    for(int i = mmboxs[mbox_idx].used; i >= 0; i--){
        mmboxs[mbox_idx].buffer[i + msg_length] = mmboxs[mbox_idx].buffer[i];
    }
    char* tmp = mmboxs[mbox_idx].buffer;
    char* t = (char*)msg;
    for(int i = 0; i < msg_length; i++){
        *tmp++ = *t++;
    }
        
    mmboxs[mbox_idx].used += msg_length;
    
    if(mmboxs[mbox_idx].empty.waiting > 0){
        do_unblock(&(mmboxs[mbox_idx].empty.cond_queue));
        mmboxs[mbox_idx].empty.waiting--;
    }

    //exit
    
    //lk.release
    if(!do_unblock(&(mmboxs[mbox_idx].lk.block_queue))) mmboxs[mbox_idx].lk.lock.status = UNLOCKED;
    
    return times;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    int times = 0;
    //lk.acquire
    if(mmboxs[mbox_idx].lk.lock.status == UNLOCKED){
        mmboxs[mbox_idx].lk.lock.status = LOCKED;
    }
    else
        do_block(&(current_running->list), &(mmboxs[mbox_idx].lk.block_queue));
    
    //critical area
    while(mmboxs[mbox_idx].used < msg_length){
        //empty.wait
        mmboxs[mbox_idx].empty.waiting++;
        
        if(!do_unblock(&(mmboxs[mbox_idx].lk.block_queue))) mmboxs[mbox_idx].lk.lock.status = UNLOCKED;
        
        do_block(&(current_running->list), &(mmboxs[mbox_idx].empty.cond_queue));
        
        if(mmboxs[mbox_idx].lk.lock.status == UNLOCKED){
            mmboxs[mbox_idx].lk.lock.status = LOCKED;
        }
        else
            do_block(&(current_running->list), &(mmboxs[mbox_idx].lk.block_queue));
            
        times++;
    }
    
    char* tmp = mmboxs[mbox_idx].buffer;
    char* t = (char*)msg;
    for(int i = 0; i < msg_length; i++){
        *t++ = *tmp++;
    }
    *t = '\0';
    for(int i = 0; i < mmboxs[mbox_idx].used - msg_length; i++){
        mmboxs[mbox_idx].buffer[i] = mmboxs[mbox_idx].buffer[i + msg_length];
    }
    
    mmboxs[mbox_idx].used -= msg_length;
    
    if(mmboxs[mbox_idx].full.waiting > 0){
        do_unblock(&(mmboxs[mbox_idx].full.cond_queue));
        mmboxs[mbox_idx].full.waiting--;
    }
    //exit
    
    //lk.release
    if(!do_unblock(&(mmboxs[mbox_idx].lk.block_queue))) mmboxs[mbox_idx].lk.lock.status = UNLOCKED;
    
    return times;
}