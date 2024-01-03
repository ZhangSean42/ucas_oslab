#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/list.h>

#define LOWBIT(x) ((x) & (-(x)))

spin_lock_t iosend_lock;
spin_lock_t iorecv_lock;
LIST_HEAD(send_block_queue);
LIST_HEAD(recv_block_queue);

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
    for (int i = 0; i < TXDESCS; i++){
        tx_desc_array[i].addr = kva2pa((uint64_t)tx_pkt_buffer[i]);
        tx_desc_array[i].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
    }
    local_flush_dcache();

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
     e1000_write_reg(e1000, E1000_TDBAH, kva2pa((uint64_t)tx_desc_array) >> 32);
     e1000_write_reg(e1000, E1000_TDBAL, kva2pa((uint64_t)tx_desc_array) << 32 >> 32);
     e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array));

	  /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */

    /* TODO: [p5-task1] Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (LOWBIT(E1000_TCTL_CT) * 0x10u) | (LOWBIT(E1000_TCTL_COLD) * 0x40u));

}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    e1000_write_reg_array(e1000, E1000_RA, 0, enetaddr[0] | enetaddr[1] << 8 | enetaddr[2] << 16 | enetaddr[3] << 24);
    e1000_write_reg_array(e1000, E1000_RA, 1, enetaddr[4] | enetaddr[5] << 8 | E1000_RAH_AV);

    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; i++){
        rx_desc_array[i].addr = kva2pa((uint64_t)rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }
    local_flush_dcache();

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
     e1000_write_reg(e1000, E1000_RDBAH, kva2pa((uint64_t)rx_desc_array) >> 32);
     e1000_write_reg(e1000, E1000_RDBAL, kva2pa((uint64_t)rx_desc_array) << 32 >> 32);
     e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDT, RXDESCS-1);

    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM);

    /* TODO: [p5-task3] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
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
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    int TDT = e1000_read_reg(e1000, E1000_TDT);
    memcpy((void*)tx_pkt_buffer[TDT], txpacket, length);
    tx_desc_array[TDT].length = length;
    tx_desc_array[TDT].status = 0;
    /*do{
        local_flush_dcache();
    }while (!(tx_desc_array[(TDT+1) % TXDESCS].status & E1000_TXD_STAT_DD));
    e1000_write_reg(e1000, E1000_TDT, (TDT+1) % TXDESCS);*/
    local_flush_dcache();
    spin_lock_acquire(&iosend_lock);
    while (!(tx_desc_array[(TDT+1) % TXDESCS].status & E1000_TXD_STAT_DD)){
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        do_block(&current_running[get_current_cpu_id()]->list, &send_block_queue, &iosend_lock);
        local_flush_dcache();
    }
    e1000_write_reg(e1000, E1000_TDT, (TDT+1) % TXDESCS);
    spin_lock_release(&iosend_lock);
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
    int RDT = e1000_read_reg(e1000, E1000_RDT);
    /*do{
        local_flush_dcache();
    }while (!(rx_desc_array[(RDT+1) % RXDESCS].status & E1000_RXD_STAT_DD));*/
    local_flush_dcache();
    spin_lock_acquire(&iorecv_lock);
    while (!(rx_desc_array[(RDT+1) % RXDESCS].status & E1000_RXD_STAT_DD)){
        do_block(&current_running[get_current_cpu_id()]->list, &recv_block_queue, &iorecv_lock);
        local_flush_dcache();
        
    }
    uint64_t length = rx_desc_array[(RDT+1) % RXDESCS].length;
    memcpy(rxbuffer, (void*)rx_pkt_buffer[(RDT+1) % RXDESCS], length);
    rx_desc_array[(RDT+1) % RXDESCS].length = 0;
    rx_desc_array[(RDT+1) % RXDESCS].status = 0;
    local_flush_dcache();
    e1000_write_reg(e1000, E1000_RDT, (RDT+1) % RXDESCS);
    spin_lock_release(&iorecv_lock);
    return length;
}

int e1000_poll_stream(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    int RDT = e1000_read_reg(e1000, E1000_RDT);
    local_flush_dcache();
    while (!(rx_desc_array[(RDT+1) % RXDESCS].status & E1000_RXD_STAT_DD)){
        do_sleep(1);
        local_flush_dcache();
    }
    do{
        local_flush_dcache();
    }while (!(rx_desc_array[(RDT+1) % RXDESCS].status & E1000_RXD_STAT_DD));
    local_flush_dcache();
    uint64_t length = rx_desc_array[(RDT+1) % RXDESCS].length;
    memcpy(rxbuffer, (void*)rx_pkt_buffer[(RDT+1) % RXDESCS], length);
    rx_desc_array[(RDT+1) % RXDESCS].length = 0;
    rx_desc_array[(RDT+1) % RXDESCS].status = 0;
    local_flush_dcache();
    e1000_write_reg(e1000, E1000_RDT, (RDT+1) % RXDESCS);
    return length;
}