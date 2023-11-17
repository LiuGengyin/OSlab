#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
     long (*func)(long, long, long) = (long (*)(long, long, long))syscall[cause];
     long value = func(regs->regs[10], regs->regs[11], regs->regs[12]);
     
     regs->regs[10] = value;
     
     regs->sepc += 4;
}