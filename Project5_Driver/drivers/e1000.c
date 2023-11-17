#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for(int i = 0; i < TXDESCS; i++){
        tx_desc_array[i].addr = 0;
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso = 0;
        tx_desc_array[i].cmd = 0;
        tx_desc_array[i].status = 0;
        tx_desc_array[i].css = 0;
        tx_desc_array[i].special = 0;
    }
    local_flush_dcache();

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    uintptr_t phyarraybase = kva2pa(tx_desc_array);
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)(phyarraybase & 0xffffffff));
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)((phyarraybase >> 32) & 0xffffffff));
    e1000_write_reg(e1000, E1000_TDLEN, (uint32_t)(sizeof(tx_desc_array)));
    printl("desclen: %d\n", sizeof(tx_desc_array));
    printl("base: %ld\n", phyarraybase);

	  /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    //e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_CT & 0x100) | (E1000_TCTL_COLD & 0x40000)); //is that right?
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    e1000_write_reg_array(e1000, E1000_RA, 0, enetaddr[0] | (enetaddr[1] << 8) | (enetaddr[2] << 16) | (enetaddr[3] << 24));
    e1000_write_reg_array(e1000, E1000_RA, 1, enetaddr[4] | (enetaddr[5] << 8) | E1000_RAH_AV);

    /* TODO: [p5-task2] Initialize rx descriptors */
    for(int i = 0; i < RXDESCS; i++){
        rx_desc_array[i].addr = 0;
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }
    local_flush_dcache();

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uintptr_t phyaddrbase = kva2pa(rx_desc_array);
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(phyaddrbase & 0xffffffff));
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)((phyaddrbase >> 32) & 0xffffffff));
    e1000_write_reg(e1000, E1000_RDLEN, (uint32_t)(sizeof(rx_desc_array)));    

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length) //传入的地址为物理地址。可以调用get_kva_of来获取内核虚地址
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);
    uint32_t head = e1000_read_reg(e1000, E1000_TDH);
    if((tail + 1) % TXDESCS == (head % TXDESCS)){
        do_block(&(current_running->list), &send_queue);
    }
    struct e1000_tx_desc* thisdesc = &tx_desc_array[tail];
    //struct e1000_tx_desc* headdesc = &tx_desc_array[head];
    thisdesc->addr = (uint64_t)txpacket;
    thisdesc->length = (uint16_t)length;
    thisdesc->cmd = 0x09;
    local_flush_dcache();
    e1000_write_reg(e1000, E1000_TDT, (tail+1) % TXDESCS);
    local_flush_dcache();

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    uint32_t tail = e1000_read_reg(e1000, E1000_RDT);
    uint32_t head = e1000_read_reg(e1000, E1000_RDH);
    if((tail + 1) % RXDESCS == (head % RXDESCS)){ //当接收描述符满载时，也进行接收进程的阻塞。
        current_running->recvdescid = -1;
        do_block(&(current_running->list), &recv_queue);
    }
    struct e1000_rx_desc* thisdesc = &rx_desc_array[tail];
    thisdesc->addr = (uint64_t)rxbuffer;
    thisdesc->length = 0;
    thisdesc->csum = 0;
    thisdesc->status = 0;
    thisdesc->errors = 0;
    thisdesc->special = 0;
    e1000_write_reg(e1000, E1000_RDT, (tail+1) % RXDESCS);
    local_flush_dcache();
    if(!(thisdesc->status & (uint8_t)0x1)){
        current_running->recvdescid = tail % RXDESCS;
        do_block(&(current_running->list), &recv_queue);
    }
    //while(!(thisdesc->status & (uint8_t)0x1));
    
    return thisdesc->length;
}

void check_net_send(){
    local_flush_dcache();
    uint32_t head = e1000_read_reg(e1000, E1000_TDH);
    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);
    if((tail + 1) % TXDESCS == (head % TXDESCS)) return;
    else{ //一次只调出一个阻塞进程即可
        do_unblock(&send_queue);
    }
}

void check_net_recv(){
    local_flush_dcache();
    uint32_t head = e1000_read_reg(e1000, E1000_RDH);
    uint32_t tail = e1000_read_reg(e1000, E1000_RDT);
    for(list_node_t* isthis = recv_queue.next; isthis != &recv_queue; ){
        pcb_t* thispcb = (pcb_t*)((char*)isthis - 32);
        list_node_t* isthisbuf;
        
        if(thispcb->recvdescid == -1){ //因为描述符不够用而阻塞
            if((tail + 1) % RXDESCS != (head % RXDESCS)){
                isthisbuf = isthis;
                isthis = isthis->next;
        
                isthisbuf->next->prev = isthisbuf->prev;
                isthisbuf->prev->next = isthisbuf->next;
            
                ready_queue.prev->next = isthisbuf;
                isthisbuf->prev = ready_queue.prev;
                isthisbuf->next = &ready_queue;
                ready_queue.prev = isthisbuf;
            }
            else isthis = isthis->next;
        }
        
        else{ //因为收不到信息而被阻塞
            if(rx_desc_array[thispcb->recvdescid].status & (uint8_t)0x1){
                isthisbuf = isthis;
                isthis = isthis->next;
        
                isthisbuf->next->prev = isthisbuf->prev;
                isthisbuf->prev->next = isthisbuf->next;
            
                ready_queue.prev->next = isthisbuf;
                isthisbuf->prev = ready_queue.prev;
                isthisbuf->next = &ready_queue;
                ready_queue.prev = isthisbuf;
            }
            else isthis = isthis->next;
        }
    }
}