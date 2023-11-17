/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
.equ kernel_stack, 0xffffffc052501000

ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t0, __bss_start
  la t1, __BSS_END__
  li t2, 0
  beq t0, t1, st
.clear_bss:
  sb t2, 0(t0)
  addi t0, t0, 1
  bne t0, t1, .clear_bss
st:
  /* TODO: [p1-task2] setup C environment */
  li sp, 0xffffffc052501000

  li t0, SR_SUM
  csrs CSR_SSTATUS, t0
  call main
loop:
  wfi
  j loop

END(_start)
