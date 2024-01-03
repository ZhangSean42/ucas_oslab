#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

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
    return 0;
}