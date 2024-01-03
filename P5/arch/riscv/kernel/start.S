/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

/* maybe you need another stack for booting? */
.equ FIRST_BOOT_STACK, 0x52001000
.equ SECOND_BOOT_STACK, 0x52002000

.section ".bootkernel_entry","ax"
ENTRY(_boot)
  /*
   * this is the entrypoint of the kernel. 
   * Don't clear BSS here
   */

  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr a0, mhartid
  bnez a0, secondary
  la sp, FIRST_BOOT_STACK
  // this will never return
  call boot_kernel

secondary:
  la sp, SECOND_BOOT_STACK
  // this will never return
  call boot_kernel


loop:
  wfi
  j loop

END(_boot)
