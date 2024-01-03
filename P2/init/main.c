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

int num_task_1 = 8;
int task_1[8] = {5, 0, 1, 2, 3, 4, 6, 7};
int pcb_count;

// Task info array
task_info_t tasks[TASK_MAXNUM];


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

void go_to_app(unsigned app_entry)
{
    asm volatile(
    "jalr %0"
    :
    :"r"(app_entry)
    :"ra");
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    uint64_t *trapframe = (uint64_t*)pt_regs;
    *(trapframe + 2)  = user_stack;
    *(trapframe + 4)  = pcb;
    pt_regs->sepc    = entry_point;
    pt_regs->sstatus = SR_SPIE; 
    pcb->user_sp = user_stack;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    uint64_t *ctx_sp = (uint64_t*)pt_switchto;
    pcb->kernel_sp = ctx_sp;
    *ctx_sp = ret_from_exception;
    *(ctx_sp + 1) = ctx_sp;
}

static void init_pcb(int tasknum)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0; i < num_task_1; i++){
        char *task_name = tasks[task_1[i]].app_name;
        unsigned task_entry_point = load_task_img(task_name, tasknum);
        ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
        ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
        pcb[i].pid = i + 2;
        pcb[i].status = TASK_READY;
        add_to_list(&ready_queue, &pcb[i].list);
        init_pcb_stack(kernel_stack, user_stack, task_entry_point, &pcb[i]);
        pcb_count++;
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    //add_to_list(&ready_queue, &pid0_pcb);
    current_running = &pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    volatile long (*(*jmptab))()   = (volatile long (*(*))())KERNEL_JMPTAB_BASE;
    syscall[SYSCALL_SLEEP]         = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]         = jmptab[YIELD];
    syscall[SYSCALL_WRITE]         = jmptab[WRITE];
    syscall[SYSCALL_CURSOR]        = jmptab[MOVE_CURSOR];
    syscall[SYSCALL_REFLUSH]       = jmptab[REFLUSH];
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = jmptab[MUTEX_INIT];
    syscall[SYSCALL_LOCK_ACQ]      = jmptab[MUTEX_ACQ];
    syscall[SYSCALL_LOCK_RELEASE]  = jmptab[MUTEX_RELEASE];
    syscall[SYSCALL_THREAD_CREATE] = (long(*)())thread_create;
    syscall[SYSCALL_THREAD_YIELD]  = (long(*)())thread_yield;
}
/************************************************************/
void w_tp(uint64_t pcb0_addr)
{
    asm volatile(
    "mv tp, %0"
    :
    :"r"(pcb0_addr));
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();
    
    // Init task information (〃'▽'〃)
    int task_num = init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb(task_num);
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    w_tp(&pid0_pcb);
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
