#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      32
#define TASK_SIZE        0x10000


#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    uint8_t  app_name[32];
    uint32_t app_img_offs;
    uint32_t app_sz;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif