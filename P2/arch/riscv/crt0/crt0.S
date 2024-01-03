#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la   t1, __bss_start
    la   t2, __BSS_END__
    
bss_clear:
    beq  t1, t2, start_main
    sb   x0, 0(t1)
    addi t1, t1, 1
    j    bss_clear
    
    /* TODO: [p1-task3] enter main function */
start_main:
    call main


    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
