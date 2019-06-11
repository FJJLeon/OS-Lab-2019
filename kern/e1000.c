#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

static struct E1000 *base;

//struct tx_desc *tx_descs;
#define N_TXDESC (PGSIZE / sizeof(struct tx_desc))
struct tx_desc tx_descs[N_TXDESC] __attribute__((aligned(16)));
char tx_buf[N_TXDESC][MAX_PKTSIZE];

int
e1000_tx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors
	memset(tx_descs, 0, sizeof(tx_descs));
	memset(tx_buf, 0, sizeof(tx_buf));
	for (int i=0; i<N_TXDESC; i++) {
		tx_descs[i].addr = PADDR(tx_buf[i]);
		tx_descs[i].status |= E1000_TX_STATUS_DD;
	}
	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	// set TD Base Address register
	base->TDBAL = PADDR(tx_descs);
	base->TDBAH = 0;
	// set TD Length register
	base->TDLEN = sizeof(tx_descs);
	// set TD head and tail register
	base->TDH = 0;
	base->TDT = 0;
	// set Transmit Control register
	base->TCTL |= E1000_TCTL_EN;
	base->TCTL |= E1000_TCTL_PSP;
	base->TCTL |= E1000_TCTL_CT_ETHER;
	base->TCTL |= E1000_TCTL_COLD_FULL_DUPLEX;
	// set Transmit Inter Packet Gap register
	base->TIPG = E1000_TIPG_DEFAULT;

	return 0;
}

struct rx_desc *rx_descs;
#define N_RXDESC (PGSIZE / sizeof(struct rx_desc))

int
e1000_rx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors
	// You should allocate some pages as receive buffer

	// Set hardward registers
	// Look kern/e1000.h to find useful definations

	return 0;
}

int
pci_e1000_attach(struct pci_func *pcif)
{
	// Enable PCI function
	// Map MMIO region and save the address in 'base;
	//cprintf("before enable base: 0x%x, size: 0x%x\n", pcif->reg_base[0], pcif->reg_size[0]);
	pci_func_enable(pcif);
	cprintf(" after enable base: 0x%x, size: 0x%x\n", pcif->reg_base[0], pcif->reg_size[0]);
	// base: 0xfebc0000, size: 0x20000
	//cprintf("sizeof E1000: 0x%x\n", sizeof(struct E1000)); // 0x3a24
	base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	cprintf("e1000 status: 0x%x\n", base->STATUS); // 0x80080783
	assert(base->STATUS == 0x80080783);
	cprintf("size tx_desc: 0x%x, N_TXDESC: 0x%x rx_desc: 0x%x\n", sizeof(struct tx_desc),N_TXDESC, sizeof(struct rx_desc));
	
	// initialize packet buffer
	e1000_tx_init();
	e1000_rx_init();

	return 0;
}

int
e1000_tx(const void *buf, uint32_t len)
{
	// Send 'len' bytes in 'buf' to ethernet
	// Hint: buf is a kernel virtual address
	if(buf == NULL || len < 0 || len > MAX_PKTSIZE)
    	return -E_INVAL;
		
	uint32_t tdt = base->TDT;
	// check the next descriptor is free
	if(!(tx_descs[tdt].status & E1000_TX_STATUS_DD))
		return -E_INVAL;// seems queue full and should retry

	// set up next descriptor
	memset(tx_buf[tdt], 0 , sizeof(tx_buf[tdt]));
	memmove(tx_buf[tdt], buf, len);
	tx_descs[tdt].length = len;
	tx_descs[tdt].cmd |= E1000_TX_CMD_EOP;
	tx_descs[tdt].cmd |= E1000_TX_CMD_RS;
	tx_descs[tdt].status &= ~E1000_TX_STATUS_DD;
	// update TDT
	base->TDT = (tdt + 1) % N_TXDESC;

	return 0;
}

int
e1000_rx(void *buf, uint32_t len)
{
	// Copy one received buffer to buf
	// You could return -E_AGAIN if there is no packet
	// Check whether the buf is large enough to hold
	// the packet
	// Do not forget to reset the decscriptor and
	// give it back to hardware by modifying RDT

	return 0;
}
