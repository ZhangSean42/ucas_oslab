#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  .align 4
  la t1, exception_handler_entry
  csrw CSR_STVEC, t1
  addi sp, sp, -8
  sd ra, 0(sp)
  /* TODO: [p2-task4] enable interrupts globally */
  jal enable_interrupt
  ld ra, 0(sp)
  addi sp, sp, 8
  jr ra

ENDPROC(setup_exception)
