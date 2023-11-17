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
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 16);
    nextpcb->status = TASK_RUNNING;
    //printl("%d\n", nextpcb->pid); 
    if(current_running->status == TASK_RUNNING)
        Enqueue(&(current_running->list), &ready_queue);
    current_running->status = TASK_READY;
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
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 16);
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
    pcb_t* nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 16);
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
    Enqueue(getthis, &ready_queue);
    return 1;
}
