
#include "fs.h"

#ifdef EVICTFLAG
#define BCSIZE 5
static void *bcarray[BCSIZE];
static uint32_t free_index = 0;
static uint32_t first_in = 0;

static void fifo_evict(void *addr, uint32_t blockno) 
{
	int r;

	assert(PGOFF(addr) == 0);
	// cache not full
	if (free_index < BCSIZE) {
		cprintf("EVICT: No. map block %d at 0x%08x\n", blockno, (uint32_t)addr);
		if ((r = sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W)) < 0)
			panic("in bc_pgfault, page alloc failed %e", r);
		if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
			panic("ide_read fail: %e", r);
		// Clear the dirty bit for the disk block page since we just read the
		// block from disk
		if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
			panic("in bc_pgfault, sys_page_map: %e", r);
		bcarray[free_index++] = addr;
	}
	else {
		void *evict_addr = bcarray[first_in];
		cprintf("EVICT: YES. evict 0x%08x with 0x%08x to map block %d\n", 
			(uint32_t)evict_addr, (uint32_t)addr, blockno);
		if (uvpt[PGNUM(evict_addr)] & PTE_D)
			flush_block(evict_addr);
		if ((r = sys_page_map(0, evict_addr, 0, addr, PTE_SYSCALL)) < 0)
			panic("map failed %e", r);
		if ((r = sys_page_unmap(0, evict_addr)) < 0)
			panic("unmap failed %e", r);
		if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
			panic("ide_read fail: %e", r);
		if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
			panic("sys_page_map: %e", r);
		bcarray[first_in] = addr;
		first_in = (first_in + 1) % BCSIZE;
	}
}
#endif

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:
	//cprintf("bc_pgfault at 0x%08x\n", addr);
	addr = ROUNDDOWN(addr, BLKSIZE);
	
	/*
	if ((r = sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("in bc_pgfault, page alloc failed %e", r);
	if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
		panic("ide_read fail: %e", r);
	*/
#ifdef EVICTFLAG
	if (blockno <= 2) {
		cprintf(" permanent block %d\n", blockno);
		if ((r = sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W)) < 0)
			panic("in bc_pgfault, page alloc failed %e", r);
		if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
			panic("ide_read fail: %e", r);
		// Clear the dirty bit for the disk block page since we just read the
		// block from disk
		if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
			panic("in bc_pgfault, sys_page_map: %e", r);
	} else {
		fifo_evict(addr, blockno);
	}
#else
	if ((r = sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W)) < 0)
			panic("in bc_pgfault, page alloc failed %e", r);
	if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
		panic("ide_read fail: %e", r);
	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);
#endif

	/*
	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);
	*/
	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	//panic("flush_block not implemented");
	int r;
	if (!va_is_mapped(addr) || !va_is_dirty(addr))
		return;
	addr = ROUNDDOWN(addr, BLKSIZE);
	/*
	for (int i=0; i<BLKSECTS; i++) {
		ide_write(blockno*8+i, addr+SECTSIZE*i, 1);
	}
	*/
	if ((r = ide_write(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
		panic("ide_write fail: %e", r);
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in flush_block, sys_page_map: %e", r);
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	// Now repeat the same experiment, but pass an unaligned address to
	// flush_block.

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");

	// Pass an unaligned address to flush_block.
	flush_block(diskaddr(1) + 20);
	assert(va_is_mapped(diskaddr(1)));

	// Skip the !va_is_dirty() check because it makes the bug somewhat
	// obscure and hence harder to debug.
	//assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}

