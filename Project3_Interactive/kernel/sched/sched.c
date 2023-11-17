#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void Enqueue(list_node_t* t, list_head* queue){
    list_node_t* origfst = queue->next;
    queue->next = t;
    t->prev = queue;
    t->next = origfst;
    origfst->prev = t;
}

list_node_t* Dequeue(list_head* queue){
    list_node_t* rear = queue->prev;
    if(rear == queue){
        //printl("error\n");
        return NULL;
    }
    rear->prev->next = rear->next;
    rear->next->prev = rear->prev;
    return rear;
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    // TODO: [p2-task1] Modify the current_running pointer.
    
    //printl("the first time\n");
    if(ready_queue.prev == &ready_queue) return;
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING;
    //printl("%d\n", nextpcb->pid); 
    if(current_running->status == TASK_RUNNING)
        Enqueue(&(current_running->list), &ready_queue);
    if(current_running->status != TASK_EXITED) current_running->status = TASK_READY;
    pcb_t* thispcb = current_running;
    current_running = nextpcb;
    //printl("dbg2\n");
    switch_to(thispcb, nextpcb);

    // TODO: [p2-task1] switch_to current_running

}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    if(ready_queue.prev == &ready_queue) return;
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    pcb_t* thispcb = current_running;
    uint64_t current_time = get_timer();
    uint64_t wake_time = current_time + (uint64_t)sleep_time;
    thispcb->wakeup_time = wake_time;
    Enqueue(&(current_running->list), &sleep_queue);
    current_running->status = TASK_READY;
    current_running = nextpcb;
    nextpcb->status = TASK_RUNNING;
    switch_to(thispcb, nextpcb);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    if(ready_queue.prev == &ready_queue) return;
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING;
    if(current_running->status == TASK_RUNNING)
        Enqueue(pcb_node, queue);
    current_running->status = TASK_BLOCKED;
    pcb_t* thispcb = current_running;
    current_running = nextpcb;
    switch_to(thispcb, nextpcb);
}

int do_unblock(list_head *queue)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_node_t* getthis = Dequeue(queue);
    if(!getthis) return 0;
    pcb_t* thispcb = (pcb_t*)((char*)getthis - 32);
    thispcb->status = TASK_READY;
    Enqueue(getthis, &ready_queue);
    return 1;
}

int do_process_show(int startline){
    screen_move_cursor(0, startline + 1);
    printk("[process table]:\n");
    int howmanylines = 0;
    int ii = 0;
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status == TASK_EXITED) continue;
        screen_move_cursor(0, startline + 2 + ii);
        if(pcb[i].status == TASK_READY)
            printk("PID : %d  STATUS: %s\n", pcb[i].pid, "READY");
        else if(pcb[i].status == TASK_RUNNING)
            printk("PID : %d  STATUS: %s\n", pcb[i].pid, "RUNNING");
        else if(pcb[i].status == TASK_BLOCKED)
            printk("PID : %d  STATUS: %s\n", pcb[i].pid, "BLOCKED");
        else
            printk("error: undefined status(pid = %d)\n", pcb[i].pid);
        howmanylines++;
        ii++;
    }
    
    printk("ready: ");
    for(list_node_t* p = ready_queue.next; p != &ready_queue; p = p->next){
        printk("%d ", ((pcb_t*)((char*)p - 32))->pid);
    }
    printk("\n");
    howmanylines++;
    
    printk("sleep: ");
    for(list_node_t* p = sleep_queue.next; p != &sleep_queue; p = p->next){
        printk("%d ", ((pcb_t*)((char*)p - 32))->pid);
    }
    printk("\n");
    howmanylines++;
    
    return (howmanylines+1);
}

int do_waitpid(pid_t pid){
    int i;
    for(i = 0; i < NUM_MAX_TASK; i++)
        if(pid == pcb[i].pid) break;
    
    if(i == NUM_MAX_TASK) return 0;

    if(current_running->status == TASK_RUNNING)
    Enqueue(&(current_running->list), &(pcb[i].wait_list));
    if(ready_queue.prev == &ready_queue) return 0;
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING;
    current_running->status = TASK_BLOCKED;
    pcb_t* thispcb = current_running;
    current_running = nextpcb;
    switch_to(thispcb, nextpcb);
    return pid;
}

void do_exit(){
    current_running->status = TASK_EXITED;
    current_running->pid = -1;
    for(int i = 0; i < 16; i++)
        current_running->lockid[i] = -1;
    while(current_running->wait_list.next != &(current_running->wait_list)){
        do_unblock(&(current_running->wait_list));
    }
    do_scheduler();
}

int do_kill(pid_t pid){
    if(pid == pid0_pcb.pid) return -1;
    if(pid == current_running->pid) return -2;
    int i;
    for(i = 0; i < NUM_MAX_TASK; i++)
        if(pcb[i].pid == pid) break;
    if(i == NUM_MAX_TASK) return 0;
    
    pcb[i].status = TASK_EXITED;
    pcb[i].pid = -1;
    pcb[i].list.next->prev = pcb[i].list.prev;
    pcb[i].list.prev->next = pcb[i].list.next;
    while(pcb[i].wait_list.next != &(pcb[i].wait_list)){
        do_unblock(&(pcb[i].wait_list));
    }
    
    for(int j = 0; j < 16; j++){
        if(pcb[i].lockid[j] != -1)
            release_for_kill(pcb[i].lockid[j]);
    }
    for(int k = 0; k < 16; k++)
        pcb[i].lockid[k] = -1;
    
    return 1;
}

pid_t do_getpid(){
    return (current_running->pid);
}