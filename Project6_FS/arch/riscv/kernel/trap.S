#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */

   la t0, exception_handler_entry
   csrw CSR_STVEC, t0

   addi sp, sp, -8
   sd ra, 0(sp)

   //call enable_preempt
   addi t0, zero, SIE_SEIE
   csrc CSR_SIE, t0
   call enable_interrupt

   ld ra, 0(sp)
   addi sp, sp, 8
   jr ra

  /* TODO: [p2-task4] enable interrupts globally */

ENDPROC(setup_exception)
