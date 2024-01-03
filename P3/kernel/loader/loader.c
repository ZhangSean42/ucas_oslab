#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <screen.h>
#include <os/sched.h>

uint64_t load_task_img(char *taskname, int tasknum)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    int i, start_sec, end_sec;
    unsigned load_addr;
     
    for (i = 0; i < tasknum; i++){
        if (!strcmp(taskname, (char*)tasks[i].app_name)){
            start_sec = tasks[i].app_img_offs / SECTOR_SIZE;
            end_sec   = (tasks[i].app_img_offs + tasks[i].app_sz) / SECTOR_SIZE;
            load_addr = 0x52000000 + (i << 16);
            bios_sd_read(load_addr, end_sec - start_sec+1, start_sec);
            memcpy(load_addr, load_addr + tasks[i].app_img_offs % SECTOR_SIZE, tasks[i].app_sz);
            return load_addr;
        }
    }
    while(1){
        screen_move_cursor(0,0);
        printk("panic: cannot find task in img");
    }
    return 0;
}

void panic(uint64_t sepc)
{
    for (int i = 0; i < TASK_MAXNUM; i++){
        if (pcb[i].status != TASK_EXITED){
            if (sepc == *(uint64_t*)(pcb[i].kernel_stack_base))
              return;
        }
    }
    if (sepc == *(uint64_t*)(pid0_pcb.kernel_stack_base))
      return;
    if (sepc == *(uint64_t*)(pid1_pcb.kernel_stack_base))
      return;
    screen_move_cursor(0,0);
    printk("sepc wrong, %ld, %ld, %ld", sepc, *(uint64_t*)(pid0_pcb.kernel_stack_base), *(uint64_t*)(pid1_pcb.kernel_stack_base));
    while (1){
    }
}

uint64_t panic1(uint64_t ra)
{
    if (ra != 0x50201b02)
        return ra;
    screen_move_cursor(0,0);
    printk("ra wrong from panic 1\n");
    while (1)
    ;
}

uint64_t panic2(uint64_t ra)
{
    if (ra != 0x50500f20)
        return ra;
    screen_move_cursor(0,0);
    printk("ra wrong from panic 2\n");
    while (1)
    ;
}