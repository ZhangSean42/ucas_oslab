#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ decompress, 0x52000400

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
  li a7, BIOS_PUTSTR
  la a0, msg
  call bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card  
   li t1, os_size_loc
   
   li a7, BIOS_SDREAD
   li a0, decompress
   lh a1, 0(t1)
   addi a2, x0, 1
   call bios_func_entry

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
   li t1, decompress
   jr t1

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a zsy bootloader...\n\r"

