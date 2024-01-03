#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ decompress, 0x59000000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.equ kernel, 0x50201000

.equ SECOND_KERNEL_STACK, 0x50502000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

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
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
   csrw CSR_SIE, zero
   csrw CSR_SIP, zero
   
   li t1, kernel
   csrw CSR_STVEC, t1
   li sp, SECOND_KERNEL_STACK
   
   li t0, SIE_SSIE
	 csrs CSR_SIE, t0

	 li t0, SR_SIE
	 csrw CSR_SSTATUS, t0
   
   li a7, BIOS_PUTSTR
   la a0, msg1
   call bios_func_entry

wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a zsy bootloader...\n\r"
msg1: .string "This is from core2, have a nice day!!!\n\r"

