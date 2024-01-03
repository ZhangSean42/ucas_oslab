#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/lock.h>

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   50
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x))

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
spin_lock_t screen_lock = {UNLOCKED};

/* cursor position */
static void vt100_move_cursor(int x, int y)
{
    // \033[y;xH
    printv("%c[%d;%dH", 27, y, x);
}

/* clear screen */
static void vt100_clear()
{
    // \033[2J
    printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor()
{
    // \033[?25l
    printv("%c[?25l", 27);
}

/* write a char */
static void screen_write_ch(char ch)
{
    if (ch == '\n')
    {
        current_running[get_current_cpu_id()]->cursor_x = 0;
        current_running[get_current_cpu_id()]->cursor_y++;
    }
    else
    {
        new_screen[SCREEN_LOC(current_running[get_current_cpu_id()]->cursor_x, current_running[get_current_cpu_id()]->cursor_y)] = ch;
        current_running[get_current_cpu_id()]->cursor_x++;
    }
}

void init_screen(void)
{
    vt100_hidden_cursor();
    vt100_clear();
    screen_clear(0, SCREEN_HEIGHT-1);
}

void screen_clear(int line1, int line2)
{
    int i, j;
    for (i = line1; i <= line2; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            new_screen[i * SCREEN_WIDTH + j] = ' ';
        }
    }
    current_running[get_current_cpu_id()]->cursor_x = 0;
    current_running[get_current_cpu_id()]->cursor_y = line1;
    screen_reflush();
}

void screen_move_cursor(int x, int y)
{
    current_running[get_current_cpu_id()]->cursor_x = x;
    current_running[get_current_cpu_id()]->cursor_y = y;
    //vt100_move_cursor(x, y);
}

void screen_write(char *buff)
{
    int i = 0;
    int l = strlen(buff);

    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void)
{
    int i, j;
    /* here to reflush screen buffer to serial port */
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            /* We only print the data of the modified location. */
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                vt100_move_cursor(j + 1, i + 1);
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    /* recover cursor position */
    vt100_move_cursor(current_running[get_current_cpu_id()]->cursor_x, current_running[get_current_cpu_id()]->cursor_y);
}

void screen_printf(char *buff)
{
    spin_lock_acquire(&screen_lock);
    screen_write(buff);
    screen_reflush();
    spin_lock_release(&screen_lock);   
}

void screen_scroll(int line1, int line2)
{
    int i, j;

    for (i = line1; i <= line2; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            old_screen[i * SCREEN_WIDTH + j] = 0;
        }
    }

    for (i = line1; i < line2; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            new_screen[i * SCREEN_WIDTH + j] = new_screen[(i + 1) * SCREEN_WIDTH + j];
        }
    }
}
