/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50501000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero
  
  csrr t0, mhartid
  bnez t0, secondary

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t1, __bss_start
  la t2,  __BSS_END__
  
clear_bss:
  beq t1, t2, set_kernel_stack
  sb x0, 0(t1)
  addi t1, t1, 1
  j clear_bss

  /* TODO: [p1-task2] setup C environment */
set_kernel_stack:
  li sp, KERNEL_STACK
secondary:  
  j main

loop:
  wfi
  j loop

END(_start)
