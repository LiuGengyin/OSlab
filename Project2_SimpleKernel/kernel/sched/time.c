#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time) //this will block the whole system for 'time' seconds. usage?
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    for(list_node_t* isthis = sleep_queue.next; isthis != &sleep_queue; ){
        pcb_t* thispcb = (pcb_t*)((char*)isthis - 16);
        list_node_t* isthisbuf;
        if(thispcb->wakeup_time <= get_timer()){
            isthisbuf = isthis;
            isthis = isthis->next;
        
            isthisbuf->next->prev = isthisbuf->prev;
            isthisbuf->prev->next = isthisbuf->next;
            
            ready_queue.prev->next = isthisbuf;
            isthisbuf->prev = ready_queue.prev;
            isthisbuf->next = &ready_queue;
            ready_queue.prev = isthisbuf; //jump the ready_queue
        }    
        else isthis = isthis->next;
        
    }
}