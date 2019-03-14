// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a list of function call frames", mon_backtrace },
	{ "time", "Display running time of the command", mon_checktime },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;
	uint32_t target_addr;
 
	// Your code here.
	// get the pointer to the retaddr on the stack = ebp + 4, within Return addr
    pret_addr = (char *)read_pretaddr();	// 0xf0110d9c
	cprintf("pre addr:%x\n", pret_addr);
	// get the pointer to the function do_overflow = addr of func
	target_addr = (uint32_t)do_overflow;	// 0xf010092d
	cprintf("target addr:%x\n", target_addr); 
	// save normal RA to RA + 4
	for (int i = 0; i < 4; i++)
    	cprintf("%*s%n\n", pret_addr[i] & 0xFF, "", pret_addr + 4 + i);
	// save target to RA
	for (int i = 0; i < 4; i++)
		cprintf("%*s%n\n", (target_addr >> (8*i)) & 0xFF, "", pret_addr + i);
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	overflow_me();
	cprintf("Stack backtrace:\n");

	uint32_t *ebp, *eip;
	uint32_t args[5];
	ebp = (uint32_t *)read_ebp();

	while ((uint32_t)ebp != 0) {
		// get return address and arguments 
		eip = (uint32_t *) *(ebp + 1); 
		for (int i=0; i<5; i++) {
			args[i] = *(ebp + i + 2);
		}
		cprintf("  eip %x ebp %x args %08x %08x %08x %08x %08x\n",
			 eip, ebp, args[0], args[1], args[2], args[3], args[4]);
		struct Eipdebuginfo info; 
		debuginfo_eip((uintptr_t)eip, &info);
		//cprintf(" what is info: file %s:%d func %s len:%d arg:%d addr:%x\n", 
		//	info.eip_file, info.eip_line, info.eip_fn_name, info.eip_fn_namelen, info.eip_fn_narg, info.eip_fn_addr);
		cprintf("\t%s:%d %.*s+%x\n", 
			info.eip_file, info.eip_line,
			info.eip_fn_namelen, info.eip_fn_name, (uint32_t)eip - (uint32_t)info.eip_fn_addr);
		ebp = (uint32_t *) *ebp;
	}

	cprintf("Backtrace success\n");
	return 0;
}

uint64_t rdtsc()
{
        uint32_t lo,hi;
        __asm__ __volatile__("rdtsc":"=a"(lo),"=d"(hi));
        return (uint64_t)hi<<32 | lo;
}

int
mon_checktime(int argc, char **argv, struct Trapframe *tf)
{
	uint64_t begin = 0, end = 1;
	int res = -1;
	//cprintf("args %d, argu %s, %s\n", argc, argv[0], argv[1]);
	char *targetcmd = argv[1];
	for (int i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(targetcmd, commands[i].name) == 0) {
			begin = rdtsc();
			res = commands[i].func(argc-1, argv+1, tf);
			end = rdtsc();
		}
	}
	if (res < 0)
		cprintf("Unknown command '%s'\n", targetcmd);
	else
		cprintf("%s cycles: %llu\n", targetcmd, end - begin);

	return res;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
