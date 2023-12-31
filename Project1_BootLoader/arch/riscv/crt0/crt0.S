#include <asm.h>

.equ tasks_free_loc, 0x52050000

ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
  la t0, __bss_start
  la t1, __BSS_END__
  li t2, 0
  beq t0, t1, st
.clear_bss:
  sw t2, 0(t0)
  addi t0, t0, 4
  bne t0, t1, .clear_bss
  
st:
  //sp -> 5
    /* TODO: [p1-task3] enter main function */
  addi sp, sp, -8 
  sd ra, 8(sp)
  jal ra, main
  ld ra, 8(sp)
stop_here:
    /* TODO: [p1-task3] finish task and return to the kernel */
    /* NOTE: You need to replace this with new mechanism in p3-task2! */
  ret
// while(1) loop, unreachable here
loop:
  wfi
  j loop

END(_start)
