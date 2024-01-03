#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
char app_name_buff[32];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
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
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    int task_num = init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    i = 0;
    unsigned app_entry;
    while (1){
        long c;
        
        if (((c = bios_getchar()) >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')){
            app_name_buff[i++] = c;
            bios_putchar(c);
        }
        else if (c == '\r'){
            bios_putchar('\n');
            app_name_buff[i] = '\0';
            app_entry = load_task_img(app_name_buff, task_num);
            if (app_entry == 0){
              i = 0;
              bzero(app_name_buff, 32);
              continue;
            }
            go_to_app(app_entry);
            i = 0;
            bzero(app_name_buff, 32);
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
