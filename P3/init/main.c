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
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/list.h>


char app_name_buff[32];
extern void ret_from_exception();

int task_num;

// Task info array
task_info_t tasks[TASK_MAXNUM];

void w_tp(uint64_t pcb_addr)
{
    asm volatile(
    "mv tp, %0"
    :
    :"r"(pcb_addr));
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
    int start_sec, end_sec;
    unsigned app_info_end;
    
    unsigned  app_info_offs = (unsigned)(*((int*)0x502001f4));
    
    unsigned  app_info_size = (unsigned)(*((uint16_t*)0x502001fa));
    
    unsigned  app_num = (unsigned)(*((uint16_t*)0x502001f8));
    
    app_info_end = app_info_offs + app_info_size * app_num;
    start_sec    = app_info_offs / SECTOR_SIZE;
    end_sec      = app_info_end  / SECTOR_SIZE;
    bios_sd_read(0x52000000, end_sec - start_sec+1, start_sec);
    memcpy((uint8_t*)tasks, 0x52000000 + app_info_offs % SECTOR_SIZE, app_info_size * app_num);
    
    return (int)app_num;
}

/************************************************************/

static void init_pcb(int tasknum)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0; i < NUM_MAX_TASK; i++){
        pcb[i].pid = -1;
        pcb[i].status = TASK_EXITED;
        pcb[i].pcb_wait.lock.status = UNLOCKED;
        pcb[i].pcb_wait.block_queue.prev = &pcb[i].pcb_wait.block_queue;
        pcb[i].pcb_wait.block_queue.next = &pcb[i].pcb_wait.block_queue;
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running[get_current_cpu_id()] = &pid0_pcb;
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
}
/************************************************************/

int main()
{
    if (get_current_cpu_id() == 0){
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();
    
        // Init task information (〃'▽'〃)
        task_num = init_task_info();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb(task_num);
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");
    
        init_barriers();
    
        init_semaphores();
    
        init_mbox();

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

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
