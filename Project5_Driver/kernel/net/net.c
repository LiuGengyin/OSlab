#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <pgtable.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    local_flush_dcache();
    //uint32_t head = e1000_read_reg(e1000, E1000_TDH);
    //uint32_t tail = e1000_read_reg(e1000, E1000_TDT);
    /*while((tail % TXDESCS) + 1 == (head % TXDESCS)){
        head = e1000_read_reg(e1000, E1000_TDH);
        tail = e1000_read_reg(e1000, E1000_TDT);
    }*/
    /*if((tail % TXDESCS) + 1 == (head % TXDESCS)){
        do_block(&(current_running->list), &(current_running->send_queue));
    }*/
    
    uintptr_t kva = get_kva_of(txpacket, current_running->pgdir);
    kva = kva2pa(kva);
    int len = e1000_transmit(kva, length);
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return len;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    local_flush_dcache();
    //uint32_t head = e1000_read_reg(e1000, E1000_RDH);
    //uint32_t tail = e1000_read_reg(e1000, E1000_RDT);
    /*while((tail % RXDESCS) + 1 == (head % RXDESCS)){
        head = e1000_read_reg(e1000, E1000_RDH);
        tail = e1000_read_reg(e1000, E1000_RDT);
    }*/
    /*if((tail % RXDESCS) + 1 == (head % RXDESCS)){ //当接收描述符满载时，也进行接收进程的阻塞。
        do_block(&(current_running->list), &(current_running->recv_queue));
    }*/
    
    uintptr_t kva = get_kva_of(rxbuffer, current_running->pgdir);
    kva = kva2pa(kva);
    int len = e1000_poll(kva);
    *pkt_lens = len;
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return len;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
}