// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
	const int childnum = 3;
	for (int i=1; i<=childnum; i++) {
		int pid = fork();
		if (pid == 0) {
			cprintf("child %d is running with priority %d\n", i, thisenv->env_pr);
			cprintf("child %d change priority from %d to %d\n", i, thisenv->env_pr, i);
			sys_set_pr(i);
			for (int j=1; j<=9; j++) {				
				cprintf("child %d yield with priority %d, iteration %d.\n", i, thisenv->env_pr, j);
				if (j==3) {
					cprintf("child %d change priority from %d to %d\n", i, i, childnum+1-i);
					sys_set_pr(childnum+1-i);
				}
				sys_yield();
			}
			break;
		}
	}
}
