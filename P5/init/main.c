/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/thread.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/net.h>
#include <os/time.h>
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/list.h>

char app_name_buff[32];
extern run_t free_list;
extern run_t paging_list;
extern void ret_from_exception();

int task_num;
int end_sec;
int swap_sec_start;

// Task info array
task_info_t tasks[TASK_MAXNUM];

void w_tp(uint64_t pcb_addr)
{
    asm volatile(
    "mv tp, %0"
    :
    :"r"(pcb_addr));
}

static void init_vm()
{
    uint64_t p;
    run_t *q;
    //build free_list
    for (p = pa2kva(PFREEMEM_START + ((MAX_RUN_NUM-1) << NORMAL_PAGE_SHIFT)); p >= pa2kva(PFREEMEM_START); p -= NORMAL_PAGE_SIZE){
        q = (run_t*)p;
        q->next = free_list.next;
        free_list.next = (uint64_t)q;
    }
    //build paging_list
    for (p = pa2kva(PFREEMEM_START - PAGE_SIZE); p >= pa2kva(PAGINGMEM_START); p -= NORMAL_PAGE_SIZE){
        q = (run_t*)p;
        q->next = paging_list.next;
        paging_list.next = (uint64_t)q;
    }
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[WRITE]           = (long (*)())screen_write;
    jmptab[REFLUSH]         = (long (*)())screen_reflush;

}

static int init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    int start_sec;
    unsigned app_info_end;
    
    unsigned  app_info_offs = (unsigned)(*((int*)0xffffffc0502001f4));
    
    unsigned  app_info_size = (unsigned)(*((uint16_t*)0xffffffc0502001fa));
    
    unsigned  app_num = (unsigned)(*((uint16_t*)0xffffffc0502001f8));
    
    app_info_end = app_info_offs + app_info_size * app_num;
    start_sec    = app_info_offs / SECTOR_SIZE;
    end_sec      = app_info_end  / SECTOR_SIZE;
    swap_sec_start = end_sec + 1;
    bios_sd_read(0x53000000, end_sec - start_sec+1, start_sec);  //use 0x5300_0000 to avoid conflict with core0 and core1 stack
    memcpy((uint8_t*)tasks, 0xffffffc053000000lu + app_info_offs % SECTOR_SIZE, app_info_size * app_num);
    
    return (int)app_num;
}

/************************************************************/

static void init_pcb()
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0; i < NUM_MAX_TASK; i++){
        pcb[i].pid = -1;
        pcb[i].status = TASK_EXITED;
        pcb[i].pcb_wait.lock.status = UNLOCKED;
        pcb[i].pcb_wait.block_queue.prev = &pcb[i].pcb_wait.block_queue;
        pcb[i].pcb_wait.block_queue.next = &pcb[i].pcb_wait.block_queue;
        pcb[i].cpu_affinity = 3;
    }
    char *argv[1] = {"shell"};
    pid_t shell_pid = do_exec("shell", 1, argv);
    do_taskset(shell_pid, 3);
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    volatile long (*(*jmptab))()   = (volatile long (*(*))())KERNEL_JMPTAB_BASE;
    syscall[SYSCALL_EXEC]          = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]          = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP]         = (long (*)())do_sleep;
    syscall[SYSCALL_KILL]          = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]       = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]            = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]        = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]         = jmptab[YIELD];
    syscall[SYSCALL_WRITE]         = jmptab[WRITE];
    syscall[SYSCALL_READCH]        = (long (*)())port_read_ch;
    syscall[SYSCALL_CURSOR]        = jmptab[MOVE_CURSOR];
    syscall[SYSCALL_REFLUSH]       = jmptab[REFLUSH];
    syscall[SYSCALL_CLEAR]         = (long (*)())screen_clear;
    syscall[SYSCALL_SCROLL]        = (long (*)())screen_scroll;
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = jmptab[MUTEX_INIT];
    syscall[SYSCALL_LOCK_ACQ]      = jmptab[MUTEX_ACQ];
    syscall[SYSCALL_LOCK_RELEASE]  = jmptab[MUTEX_RELEASE];
    syscall[SYSCALL_THREAD_CREATE] = (long(*)())thread_create;
    syscall[SYSCALL_THREAD_YIELD]  = (long(*)())thread_yield;
    syscall[SYSCALL_SEMA_INIT]     = (long(*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]       = (long(*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]     = (long(*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]  = (long(*)())do_semaphore_destroy;
    syscall[SYSCALL_BARR_INIT]     = (long(*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]     = (long(*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]  = (long(*)())do_barrier_destroy;
    syscall[SYSCALL_MBOX_OPEN]     = (long(*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]    = (long(*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]     = (long(*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]     = (long(*)())do_mbox_recv;
    syscall[SYSCALL_PRINTF]        = (long(*)())screen_printf;
    syscall[SYSCALL_TASKSET]       = (long(*)())do_taskset;
    syscall[SYSCALL_PTHREAD_CREATE]= (long(*)())do_pthread_create;
    syscall[SYSCALL_SHM_GET]       = (long(*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]        = (long(*)())shm_page_dt;
    syscall[SYSCALL_FORK]          = (long(*)())do_fork;
    syscall[SYSCALL_NET_SEND]      = (long(*)())do_net_send;
    syscall[SYSCALL_NET_RECV]      = (long(*)())do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM] = (long(*)())do_net_recv_stream;
}
/************************************************************/

int main()
{   
    if (get_current_cpu_id() == 0){
        
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();
    
        // Init task information (〃'▽'〃)
        task_num = init_task_info();
        
        // Init allocable page
        init_vm();
        
        /* TODO: [p2-task1] remember to initialize 'current_running' */
        current_running[get_current_cpu_id()] = &pid0_pcb;

        // Read Flatten Device Tree (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);
        e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);
        
        // IOremap
        plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");
        
        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");
    
        init_barriers();
    
        init_semaphores();
    
        init_mbox();
        
        init_stream_array();

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");
        
        // TODO: [p5-task3] Init plic
        plic_init(plic_addr, nr_irqs);
        printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");
        
        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");
        
        w_tp(&pid0_pcb);
        
        send_ipi((unsigned long *)0);
        clear_sip();
    }
    else{
        current_running[get_current_cpu_id()] = &pid1_pcb;
        setup_exception();
        w_tp(&pid1_pcb);
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer((uint64_t)(get_ticks() + TIMER_INTERVAL));
    enable_preempt();
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        //do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        asm volatile("wfi");
    }

    return 0;
}
