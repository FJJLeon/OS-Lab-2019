#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted EP_FUNCtion addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

void EP_NONE() {};

void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	// Exception handler      // exception number
	extern void EP_DIVIDE (); /* 0 */
	extern void EP_DEBUG  (); /* 1 */
	extern void EP_NMI    (); /* 2 */
	extern void EP_BRKPT  (); /* 3 */
	extern void EP_OFLOW  (); /* 4 */
	extern void EP_BOUND  (); /* 5 */
	extern void EP_ILLOP  (); /* 6 */
	extern void EP_DEVICE (); /* 7 */
	extern void EP_DBLFLT (); /* 8 */
		   
	extern void EP_TSS    (); /* 10 */
	extern void EP_SEGNP  (); /* 11 */
	extern void EP_STACK  (); /* 12 */
	extern void EP_GPFLT  (); /* 13 */
	extern void EP_PGFLT  (); /* 14 */
		   
	extern void EP_FPERR  (); /* 16 */
	extern void EP_ALIGN  (); /* 17 */
	extern void EP_MCHK   (); /* 18 */
	extern void EP_SIMDERR(); /* 19 */

	extern void EP_SYSCALL(); /* 48 */

	// IRQ handler 					// irq number
	extern void EP_IRQ_TIMER (); 	// 0
	extern void EP_IRQ_KBD   (); 	// 1
	extern void EP_IRQ_2     (); 	// 2
	extern void EP_IRQ_3     (); 	// 3
	extern void EP_IRQ_SERIAL(); 	// 4
	extern void EP_IRQ_5     (); 	// 5
	extern void EP_IRQ_6     (); 	// 6
	extern void EP_IRQ_SPURIOUS(); 	// 7
	extern void EP_IRQ_8 	 (); 	// 8
	extern void EP_IRQ_9 	 (); 	// 9
	extern void EP_IRQ_10	 (); 	// 10
	extern void EP_IRQ_11	 (); 	// 11
	extern void EP_IRQ_12	 (); 	// 12
	extern void EP_IRQ_13 	 (); 	// 13
	extern void EP_IRQ_IDE	 (); 	// 14
	extern void EP_IRQ_15 	 (); 	// 15
	extern void EP_IRQ_ERROR (); 	// 19
	
	void (*EP_FUNC[]) () = {
		EP_DIVIDE, EP_DEBUG, EP_NMI  , EP_BRKPT  ,
		EP_OFLOW , EP_BOUND, EP_ILLOP, EP_DEVICE ,
		EP_DBLFLT, EP_NONE , EP_TSS  , EP_SEGNP  ,
		EP_STACK , EP_GPFLT, EP_PGFLT, EP_NONE   ,
		EP_FPERR , EP_ALIGN, EP_MCHK , EP_SIMDERR,
	};
	// use SETGATE to set idt
	for (int i=0; i<20; i++) {
		if (i == 9 || i == 15)
			continue;
		unsigned gd_dpl = 0;
		if (i == T_BRKPT) 
			gd_dpl = 3;
		SETGATE(idt[i], 0, GD_KT, EP_FUNC[i], gd_dpl);
	}

	void (*IRQ_FUNC[]) () = {
		EP_IRQ_TIMER, EP_IRQ_KBD, EP_IRQ_2,  EP_IRQ_3,
		EP_IRQ_SERIAL,EP_IRQ_5,   EP_IRQ_6,  EP_IRQ_SPURIOUS,
		EP_IRQ_8,     EP_IRQ_9,   EP_IRQ_10, EP_IRQ_11,
		EP_IRQ_12,    EP_IRQ_13,  EP_IRQ_IDE,EP_IRQ_15,
	};
	
	for (int irqn=0; irqn<16; irqn++) {
		SETGATE(idt[IRQ_OFFSET + irqn], 0, GD_KT, IRQ_FUNC[irqn], 0);
	}
	// ERROR IRQ = 19
	SETGATE(idt[IRQ_OFFSET + IRQ_ERROR], 0, GD_KT, EP_IRQ_ERROR, 0);

	// set syscall idt
	SETGATE(idt[T_SYSCALL], 0, GD_KT, EP_SYSCALL, 3);
	//extern void sysenter_handler();
  	//wrmsr(0x174, GD_KT, 0);           /* SYSENTER_CS_MSR */
  	//wrmsr(0x175, KSTACKTOP, 0);       /* SYSENTER_ESP_MSR */
	//wrmsr(0x176, sysenter_handler, 0);/* SYSENTER_EIP_MSR */

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
	//     from doing IO (0 is not the correct value!)
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:
	int id = cpunum();
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - id * (KSTKSIZE + KSTKGAP);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	// CPU i's TSS descriptor
	gdt[(GD_TSS0 >> 3) + id] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + id].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + (id << 3));
	// Load the IDT
	lidt(&idt_pd);

	//extern void sysenter_handler();
	//wrmsr(0x174, GD_KT, 0);                   /* SYSENTER_CS_MSR */
	//wrmsr(0x175, thiscpu->cpu_ts.ts_esp0 , 0);/* SYSENTER_ESP_MSR */
	//wrmsr(0x176, sysenter_handler, 0);        /* SYSENTER_EIP_MSR */
	
	//cprintf("trap_init percpu return in cpu %d\n", id);
	/*
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;
	ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);*/
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	switch(tf->tf_trapno) {
		case T_BRKPT:
			monitor(tf);
			return;
		case T_PGFLT:
			//cprintf("page fault\n");
			page_fault_handler(tf);
			break;
		case T_SYSCALL:
			//cprintf("syscall\n");
			tf->tf_regs.reg_eax = syscall(
					tf->tf_regs.reg_eax,
					tf->tf_regs.reg_edx,
					tf->tf_regs.reg_ecx,
					tf->tf_regs.reg_ebx,
					tf->tf_regs.reg_edi,
					tf->tf_regs.reg_esi 
				);
			return;
		default:
			//cprintf("trap num : %d\n", tf->tf_trapno);
			break;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		time_tick();
		lapic_eoi();
		sched_yield();
		return;
	}

	// Add time tick increment to clock interrupts.
	// Be careful! In multiprocessors, clock interrupts are
	// triggered on every CPU.
	// LAB 6: Your code here.

	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
		kbd_intr();
		return;
	}
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
		serial_intr();
		return;
	}

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		assert(curenv);
		lock_kernel();

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if (0 == (tf->tf_cs & 0x3))
		panic("kernel page fault");
	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// It is convenient for our code which returns from a page fault
	// (lib/pfentry.S) to have one word of scratch space at the top of the
	// trap-time stack; it allows us to more easily restore the eip/esp. In
	// the non-recursive case, we don't have to worry about this because
	// the top of the regular user stack is free.  In the recursive case,
	// this means we have to leave an extra word between the current top of
	// the exception stack and the new stack frame because the exception
	// stack _is_ the trap-time stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	if (curenv->env_pgfault_upcall) {
		struct UTrapframe *utf;
		const size_t utfsize = sizeof(struct UTrapframe);
		if (UXSTACKTOP - tf->tf_esp < PGSIZE)
			utf = (struct UTrapframe *) (tf->tf_esp - utfsize - 4);
		else
			utf = (struct UTrapframe *) (UXSTACKTOP - utfsize);
		user_mem_assert(curenv, (void *)utf, utfsize, PTE_W);

		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_trapno;
		utf->utf_regs = tf->tf_regs;
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;
		
		// switch to user exception stack
		curenv->env_tf.tf_eip = (uintptr_t) curenv->env_pgfault_upcall;
		curenv->env_tf.tf_esp = (uintptr_t) utf;
		env_run(curenv);
	}
	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

