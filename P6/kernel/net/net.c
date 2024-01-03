#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

#define PROTOCAL_START 54
#define DAT 0x01
#define RSD 0x02
#define ACK 0x04
#define STREAM_ARRAY_SIZE 128
static char tmp_buffer[2048];
static char tx_buff[128];

typedef struct stream_list{
    int seq;
    int len;
    int prev;
    int next;
    int valid;
} stream_list_t;
stream_list_t stream[STREAM_ARRAY_SIZE];

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    length = e1000_transmit(txpacket, length);
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full

    return length;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    int offset = 0;
    for (int i = 0; i < pkt_num; i++){
        int length = e1000_poll(rxbuffer+offset);
        pkt_lens[i] = length;
        offset += length;
    }
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return offset;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task3] Handle interrupts from network device
    uint32_t ICR = e1000_read_reg(e1000, E1000_ICR);
    uint32_t IMS = e1000_read_reg(e1000, E1000_IMS);
    if ((ICR & E1000_ICR_TXQE) && (IMS & E1000_IMS_TXQE)){
        spin_lock_acquire(&iosend_lock);
        while (send_block_queue.next != &send_block_queue){
            e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
            do_unblock(send_block_queue.next); 
        }  
        spin_lock_release(&iosend_lock);
    }
    if ((ICR & E1000_ICR_RXDMT0) && (IMS & E1000_IMS_RXDMT0)){
        spin_lock_acquire(&iorecv_lock);
        while (recv_block_queue.next != &recv_block_queue){
            do_unblock(recv_block_queue.next); 
        }  
        spin_lock_release(&iorecv_lock);
    }
}

void init_stream_array()
{
    for (int i = 0; i < STREAM_ARRAY_SIZE; i++){
        stream[i].prev = -1;
        stream[i].next = -1;
        stream[i].seq  = 0;
        stream[i].len  = 0;
        if (i == 0)
            stream[i].valid = 1;
        else
            stream[i].valid = 0;
    }
}

void do_net_recv_stream(char *buffer, int *nbytes)
{
    int bytes = *nbytes;
    while (bytes > 0){
        //e1000_poll(tmp_buffer);
        e1000_poll_stream(tmp_buffer);
        char *magic_pos = tmp_buffer+PROTOCAL_START;
        char *type_pos  = tmp_buffer+PROTOCAL_START+1;
        if (*magic_pos == 0x45 && (*type_pos & DAT) == 1){
            memcpy(tx_buff, tmp_buffer, 54);
            short len0 = tmp_buffer[PROTOCAL_START+3];
            short len1 = tmp_buffer[PROTOCAL_START+2];
            short len  = (len1 << 8) + len0;
            
            int seq0 = tmp_buffer[PROTOCAL_START+7];
            int seq1 = tmp_buffer[PROTOCAL_START+6];
            int seq2 = tmp_buffer[PROTOCAL_START+5];
            int seq3 = tmp_buffer[PROTOCAL_START+4];
            int seq  = (seq3 << 24) + (seq2 << 16) + (seq1 << 8) + seq0;
            
            memcpy(buffer+seq, tmp_buffer+PROTOCAL_START+8, len);
            
            int p, q, r;
            int size = 0;
            for (p = 0; p != -1 && !(stream[p].seq <= (seq+len) && seq <= (stream[p].seq+stream[p].len)); p = stream[p].next)
                ;
            if (p == -1){
                for (p = 0; stream[p].next != -1 && stream[stream[p].next].seq < seq; p = stream[p].next)
                    ;
                int i = 0;
                while (stream[i].valid){
                    i++;
                }
                stream[i].seq   = seq;
                stream[i].len   = len;
                stream[i].next  = stream[p].next; 
                stream[i].prev  = p;
                stream[i].valid = 1; 
                if (stream[p].next != -1){
                    stream[stream[p].next].prev = i;
                }
                stream[p].next  = i;
                bytes -= len;
            }
            else{
                for (q = 0; q != -1 && stream[q].seq <= (seq+len); q = stream[q].next)
                    ;
                int start, end;
                for (r = p; r != q; r = stream[r].next){
                    if (r == p){
                        start = seq < stream[r].seq ? seq : stream[r].seq; 
                    }
                    else{
                        stream[r].valid = 0;
                    }
                    if (stream[r].next == q){
                        end = (seq+len) > (stream[r].seq+stream[r].len) ? (seq+len) : (stream[r].seq+stream[r].len);
                    }
                    size += stream[r].len;
                }
                stream[p].seq  = start;
                stream[p].len  = end - start;
                stream[p].next = q;
                if (q != -1){
                    stream[q].prev = p;
                } 
                bytes = bytes + size - stream[p].len;
            }
          for (int j = 0; j != -1; j = stream[j].next){
              if (stream[j].len){
                  printl("%d seq%d len%d\n", j, stream[j].seq, stream[j].len);
              }
          }
          printl("\n");
        }
        else
          continue;
    }
}

int RSD_ptr = 0;
void do_RSD()
{
    int RSD_req_start;
    
    tx_buff[PROTOCAL_START] = 0x45;
    /*for (int p = 0; p != -1; p = stream[p].next){
        tx_buff[PROTOCAL_START+1] = RSD;
        int req_start = stream[p].seq + stream[p].len;
        tx_buff[PROTOCAL_START+4] =  req_start >> 24;
        tx_buff[PROTOCAL_START+5] = (req_start & 0x00ff0000) >> 16;
        tx_buff[PROTOCAL_START+6] = (req_start & 0x0000ff00) >> 8;
        tx_buff[PROTOCAL_START+7] =  req_start & 0x000000ff;
        e1000_transmit(tx_buff, 62);
    }*/

    tx_buff[PROTOCAL_START+1] = RSD;
    RSD_req_start = stream[RSD_ptr].seq + stream[RSD_ptr].len;
    tx_buff[PROTOCAL_START+4] =  RSD_req_start >> 24;
    tx_buff[PROTOCAL_START+5] = (RSD_req_start & 0x00ff0000) >> 16;
    tx_buff[PROTOCAL_START+6] = (RSD_req_start & 0x0000ff00) >> 8;
    tx_buff[PROTOCAL_START+7] =  RSD_req_start & 0x000000ff;
    e1000_transmit(tx_buff, 62);
    /*RSD_ptr = stream[RSD_ptr].next;
    if (RSD_ptr == -1)
        RSD_ptr = 0;*/
    printl("RSD seq%d\n", RSD_req_start);
    printl("\n");
}

int ACK_ptr = 0;
void do_ACK()
{   
    int ACK_req_start;
    
    if (stream[0].seq == 0 && stream[0].len > ACK_ptr){
        tx_buff[PROTOCAL_START+1] = ACK;
        ACK_req_start = stream[0].seq + stream[0].len;
        ACK_ptr = ACK_req_start;
        tx_buff[PROTOCAL_START+4] =  ACK_req_start >> 24;
        tx_buff[PROTOCAL_START+5] = (ACK_req_start & 0x00ff0000) >> 16;
        tx_buff[PROTOCAL_START+6] = (ACK_req_start & 0x0000ff00) >> 8;
        tx_buff[PROTOCAL_START+7] =  ACK_req_start & 0x000000ff;
        e1000_transmit(tx_buff, 62);
        printl("ACK seq%d\n", ACK_req_start);
        printl("\n");
    }
}