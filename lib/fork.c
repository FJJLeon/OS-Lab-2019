// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0)
		panic("pgfault, page fault not caused by write");
	if ((uvpt[PGNUM(addr)] & PTE_COW) == 0)
		panic("pgfault, page fault not COW page");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	//panic("pgfault not implemented");
	// alloc tmp page
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault, page alloc failed %e", r);
	addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, addr, PGSIZE);
	if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault, can not map temp to origin page %e", r);
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("pgfault, page unmap failed %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	// panic("duppage not implemented");
	void *addr = (void *)(pn * PGSIZE);
	pte_t pte = uvpt[pn];
	int perm = PTE_U | PTE_P;
	if (pte & (PTE_W | PTE_COW))
		perm |= PTE_COW;
	if (pte & PTE_SHARE) {
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, pte & PTE_SYSCALL)) < 0)
			panic("duppage, shared page map child fail %e", r);
	} 
	else {
		// map to child va
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, perm)) < 0)
			panic("duppage, page map child fail %e", r);
		// if COW, remap our mapping
		if (perm & PTE_COW) {
			if ((r = sys_page_map((envid_t)0, addr, 0, addr, perm)) < 0)
				panic("duppage, page map self fail %e", r);
		}
	}
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	// panic("fork not implemented");
	set_pgfault_handler(pgfault);

	extern void _pgfault_upcall(void);

	envid_t envid;
	uintptr_t addr;
	int r;

	if ((envid = sys_exofork()) < 0)
		panic("fork, exofork fail");
	if (envid == 0) {
		// child process, fix thisenv
		thisenv = &envs[ENVX(sys_getenvid())];
		//cprintf("fork getenvid in child: %08x\n", sys_getenvid());
		sys_set_pr(0);
		return envid; // 0
	}
	// parent process
	// copy address space to child
	for (addr=UTEXT; addr<USTACKTOP; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & (PTE_P|PTE_U)) == (PTE_P|PTE_U))
			duppage(envid, PGNUM(addr));
	// create user exception stack
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("fork, alloc exception stack fail %e", r);
	// set user page fault entrypoint
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("fork, set pgfault upcall fail %e", r);
	// set RUNNABLE
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork, set child status fail %e", r);
	// parent return child envid
	return envid;
}	

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
