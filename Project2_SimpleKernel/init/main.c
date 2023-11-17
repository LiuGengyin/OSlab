#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

#define TASK_NUM_LOC 0x502001fe

// Task info array
task_info_t tasks[TASK_MAXNUM];

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
      /* TODO: [p2-task3] initialization of registers on kernel stack
       * HINT: sp, ra, sepc, sstatus
       * NOTE: To run the task in user mode, you should set corresponding bits
       *     of sstatus(SPP, SPIE, etc.).
       */
     regs_context_t *pt_regs =
         (regs_context_t *)(kernel_stack - sizeof(regs_context_t));//user context
         
     pt_regs->sstatus = SR_SPIE;
     pt_regs->sepc = entry_point;
     //pt_regs->sbadaddr = ?;
     //pt_regs->scause = ?;
     pcb->user_sp = user_stack;
         
     /* TODO: [p2-task1] set sp to simulate just returning from switch_to
      * NOTE: you should prepare a stack, and push some values to
      * simulate a callee-saved context.
      */
     switchto_context_t *pt_switchto =
         (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));//kernel context

    pt_switchto->regs[0] = ret_from_exception;//?
    pt_switchto->regs[1] = (reg_t)pt_switchto;
    pcb->kernel_sp = (reg_t)pt_switchto;
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    pid0_pcb.status = TASK_READY;
    pid0_pcb.kernel_sp = allocKernelPage(1);
    pid0_pcb.pid = 0;
    init_pcb_stack(pid0_pcb.kernel_sp, pid0_pcb.user_sp, 0, &pid0_pcb);
    
    for(int i = 0; i < 7; i++){
        pcb[i].kernel_sp = allocKernelPage(1);
        pcb[i].user_sp = allocUserPage(1);
        pcb[i].pid = i+1;
        pcb[i].status = TASK_READY;
        Enqueue(&(pcb[i].list), &ready_queue);
        char name[20];
        if(i == 0) strcpy(name, "print1");
        else if(i == 1) strcpy(name, "print2");
        else if(i == 2) strcpy(name, "fly");
        else if(i == 3) strcpy(name, "lock1");
        else if(i == 4) strcpy(name, "lock2");
        else if(i == 5) strcpy(name, "timer");
        else if(i == 6) strcpy(name, "sleep");
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, load_task_img(name), &pcb[i]);
    }
    
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
    asm volatile("addi tp, %0, 0\t\n"
                 :
                 : "r" (current_running));
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's


    //here are my work in P1. do I still need them?
    
    /*char* whichtask = (char*)0x502001f4;
    char* whichtask1 = whichtask;
    
    for(int i = 0; i < 3; i++){
        bios_putchar(*(whichtask1++));
        bios_putchar('\n');
    }
    
    for(int i = 0; i < 3; i++){
        if(*whichtask == '1')
            load_task_img("print1");
        else if(*whichtask == '2')
            load_task_img("print2");
        else if(*whichtask == '3')
            load_task_img("fly");
        else
            bios_putstr("error here\n");
        whichtask++;
        //bios_putstr("loop\n");
    }*/


    set_timer(get_ticks() + TIMER_INTERVAL);

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
