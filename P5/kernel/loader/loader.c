#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <type.h>
#include <screen.h>
#include <os/sched.h>

uint64_t load_task_img(char *taskname, int task_num, int pcb_idx)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    int start_sector, end_sector;
    uint64_t load_addr;
     
    for (int i = 0; i < task_num; i++){
        if (!strcmp(taskname, (char*)tasks[i].app_name)){
            init_uvm(pcb_idx, tasks[i].mem_sz);
            start_sector = tasks[i].app_img_offs / SECTOR_SIZE;
            end_sector   = (tasks[i].app_img_offs + tasks[i].app_sz) / SECTOR_SIZE;
            load_addr = 0x59000000;
            bios_sd_read(load_addr, end_sector - start_sector+1, start_sector);
            memcpy(pa2kva(load_addr), pa2kva(load_addr) + tasks[i].app_img_offs % SECTOR_SIZE, tasks[i].app_sz);
            for (uint64_t sz = 0; sz < tasks[i].mem_sz; sz += PAGE_SIZE){
                PTE *p = walk(USR_START+sz, (PTE*)pcb[pcb_idx].pgdir);
                uint64_t kva = pa2kva(get_pfn(*p));
                memcpy(kva, pa2kva(load_addr+sz), PAGE_SIZE);
            }
            uint64_t *p = 0xffffffc053007000;
            printl("value at 0x53007000  %lx\n", *p);
            return tasks[i].mem_sz;
        }
    }
    while(1){
        screen_move_cursor(0,0);
        printk("panic: cannot find task in img");
    }
    return 0;
}