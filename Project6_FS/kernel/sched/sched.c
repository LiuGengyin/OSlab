#include <e1000.h>
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <pgtable.h>
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
LIST_HEAD(send_queue);
LIST_HEAD(recv_queue);

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
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    //check_net_send();
    //check_net_recv();
    // TODO: [p2-task1] Modify the current_running pointer.
    
    //printl("the first time\n");
    pcb_t* thispcb;
    pcb_t* nextpcb;
    if(ready_queue.next == &ready_queue)
        nextpcb = &pid0_pcb;
    else nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING; 
    if(current_running->status == TASK_RUNNING)
        Enqueue(&(current_running->list), &ready_queue);
    if(current_running->status != TASK_EXITED) current_running->status = TASK_READY;
    thispcb = current_running;
    current_running = nextpcb;
    set_satp(SATP_MODE_SV39, nextpcb->pid, kva2pa(nextpcb->pgdir) >> NORMAL_PAGE_SHIFT); //切换页表。事实上在switch_to的前半段还是需要thispcb的页表，但是由于全部是内核部分页表管理的映射，切换页表也不会改变映射规则。
    local_flush_tlb_all();
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
    pcb_t* nextpcb;
    if(ready_queue.next == &ready_queue)
        nextpcb = &pid0_pcb;
    else nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    pcb_t* thispcb = current_running;
    uint64_t current_time = get_timer();
    uint64_t wake_time = current_time + (uint64_t)sleep_time;
    thispcb->wakeup_time = wake_time;
    Enqueue(&(current_running->list), &sleep_queue);
    current_running->status = TASK_READY;
    current_running = nextpcb;
    nextpcb->status = TASK_RUNNING;
    set_satp(SATP_MODE_SV39, 0, kva2pa(nextpcb->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    switch_to(thispcb, nextpcb);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t* nextpcb;
    if(ready_queue.next == &ready_queue)
        nextpcb = &pid0_pcb;
    else nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING;
    if(current_running->status == TASK_RUNNING)
        Enqueue(pcb_node, queue);
    current_running->status = TASK_BLOCKED;
    pcb_t* thispcb = current_running;
    current_running = nextpcb;
    set_satp(SATP_MODE_SV39, nextpcb->pid, kva2pa(nextpcb->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
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
    pcb_t* nextpcb;
    if(ready_queue.next == &ready_queue)
        nextpcb = &pid0_pcb;
    else nextpcb = (pcb_t*)((char*)Dequeue(&ready_queue) - 32);
    nextpcb->status = TASK_RUNNING;
    current_running->status = TASK_BLOCKED;
    pcb_t* thispcb = current_running;
    current_running = nextpcb;
    set_satp(SATP_MODE_SV39, nextpcb->pid, kva2pa(nextpcb->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
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
    
    //释放内存
    /*for(int i = 0; i < 255; i++){
        PTE* pointer = current_running->pgdir;
        if(pointer[i] != 0){
            PTE* pgdir2 = get_pa(pointer[i]);
            pgdir2 = pa2kva(pgdir2);
            for(int j = 0; j < 512; j++){
                if(pgdir2[j] != 0){
                    PTE* pgtb = get_pa(pgdir2[j]);
                    pgtb = pa2kva(pgtb);
                    for(int k = 0; k < 512; k++){
                        if(pgtb[k] != 0){
                            uintptr_t va = pa2kva(get_pa(pgtb[k]));
                            freePage(va);
                        } //释放三级页表中所有页
                    }
                    freePage(pgtb);
                } //释放三级页表
            }
            freePage(pgdir2);
        } //释放用户二级页表
    }
    
    freePage(current_running->kernel_stack_base); //释放内核栈
    //暂不释放用户一级页表
    */
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
        
    //释放内存
    /*for(int m = 0; m < 255; m++){
        PTE* pointer = pcb[i].pgdir;
        if(pointer[m] != 0){
            PTE* pgdir2 = get_pa(pointer[m]);
            pgdir2 = pa2kva(pgdir2);
            for(int j = 0; j < 512; j++){
                if(pgdir2[j] != 0){
                    PTE* pgtb = get_pa(pgdir2[j]);
                    pgtb = pa2kva(pgtb);
                    for(int k = 0; k < 512; k++){
                        if(pgtb[k] != 0){
                            uintptr_t va = pa2kva(get_pa(pgtb[k]));
                            freePage(va);
                        } //释放三级页表中所有页
                    }
                    freePage(pgtb);
                } //释放三级页表
            }
            freePage(pgdir2);
        } //释放用户二级页表
    }
    
    freePage(pcb[i].kernel_stack_base); //释放内核栈
    //暂不释放用户一级页表
    */
    return 1;
}

pid_t do_getpid(){
    return (current_running->pid);
}