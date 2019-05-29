/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(EP_DIVIDE , T_DIVIDE )  /* 0 */
TRAPHANDLER_NOEC(EP_DEBUG  , T_DEBUG  )  /* 1 */
TRAPHANDLER_NOEC(EP_NMI    , T_NMI    )  /* 2 */
TRAPHANDLER_NOEC(EP_BRKPT  , T_BRKPT  )  /* 3 */
TRAPHANDLER_NOEC(EP_OFLOW  , T_OFLOW  )  /* 4 */
TRAPHANDLER_NOEC(EP_BOUND  , T_BOUND  )  /* 5 */
TRAPHANDLER_NOEC(EP_ILLOP  , T_ILLOP  )  /* 6 */
TRAPHANDLER_NOEC(EP_DEVICE , T_DEVICE )  /* 7 */
TRAPHANDLER     (EP_DBLFLT , T_DBLFLT )  /* 8 */

TRAPHANDLER     (EP_TSS    , T_TSS    )  /* 10 */
TRAPHANDLER		(EP_SEGNP  , T_SEGNP  )  /* 11 */
TRAPHANDLER     (EP_STACK  , T_STACK  )  /* 12 */
TRAPHANDLER     (EP_GPFLT  , T_GPFLT  )  /* 13 */
TRAPHANDLER     (EP_PGFLT  , T_PGFLT  )  /* 14 */

TRAPHANDLER_NOEC(EP_FPERR  , T_FPERR  )  /* 16 */
TRAPHANDLER_NOEC(EP_ALIGN  , T_ALIGN  )  /* 17 */
TRAPHANDLER_NOEC(EP_MCHK   , T_MCHK   )  /* 18 */
TRAPHANDLER_NOEC(EP_SIMDERR, T_SIMDERR)  /* 19 */

TRAPHANDLER_NOEC(EP_SYSCALL, T_SYSCALL)  /* 48 */
/* TRAPHANDLER                                         IRQ NUMBER*/
TRAPHANDLER_NOEC(EP_IRQ_TIMER   , IRQ_OFFSET+IRQ_TIMER   )  /*  0*/
TRAPHANDLER_NOEC(EP_IRQ_KBD     , IRQ_OFFSET+IRQ_KBD     )  /*  1*/
TRAPHANDLER_NOEC(EP_IRQ_2       , IRQ_OFFSET+    2       )  /*  2*/
TRAPHANDLER_NOEC(EP_IRQ_3       , IRQ_OFFSET+    3       )  /*  3*/
TRAPHANDLER_NOEC(EP_IRQ_SERIAL  , IRQ_OFFSET+IRQ_SERIAL  )  /*  4*/
TRAPHANDLER_NOEC(EP_IRQ_5       , IRQ_OFFSET+    5       )  /*  5*/
TRAPHANDLER_NOEC(EP_IRQ_6       , IRQ_OFFSET+    6       )  /*  6*/
TRAPHANDLER_NOEC(EP_IRQ_SPURIOUS, IRQ_OFFSET+IRQ_SPURIOUS)  /*  7*/
TRAPHANDLER_NOEC(EP_IRQ_8       , IRQ_OFFSET+    8       )  /*  8*/
TRAPHANDLER_NOEC(EP_IRQ_9       , IRQ_OFFSET+    9       )  /*  9*/
TRAPHANDLER_NOEC(EP_IRQ_10      , IRQ_OFFSET+    10      )  /* 10*/
TRAPHANDLER_NOEC(EP_IRQ_11      , IRQ_OFFSET+    11      )  /* 11*/
TRAPHANDLER_NOEC(EP_IRQ_12      , IRQ_OFFSET+    12      )  /* 12*/
TRAPHANDLER_NOEC(EP_IRQ_13      , IRQ_OFFSET+    13      )  /* 13*/
TRAPHANDLER_NOEC(EP_IRQ_IDE     , IRQ_OFFSET+IRQ_IDE     )  /* 14*/
TRAPHANDLER_NOEC(EP_IRQ_15      , IRQ_OFFSET+    15      )  /* 15*/
TRAPHANDLER_NOEC(EP_IRQ_ERROR   , IRQ_OFFSET+IRQ_ERROR   )  /* 19*/
/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
	pushl %ds
	pushl %es
	pushal
	pushl $GD_KD
	popl %ds
	pushl $GD_KD
	popl %es
	pushl %esp
	call trap

.globl sysenter_handler;							
.type sysenter_handler, @function;						
.align 2;
sysenter_handler:
	pushl %edi
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %eax
	call syscall
	movl %ebp, %ecx
	movl %esi, %edx
	sysexit