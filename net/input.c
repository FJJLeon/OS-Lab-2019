#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server (using ipc_send with NSREQ_INPUT as value)
	//	do the above things in a loop
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	int r, len;
	char buf[2048];
	while(1) {
		// read a packet from the device driver
		if((len = sys_net_recv(buf, 2048)) < 0) {
			sys_yield();
			continue;
		}
		// alloc new page for buf, or same addr pkt corrupt
		if ((r = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_W|PTE_U)) < 0)
			panic("input env: page alloc fail %e", r);

		// send it to the network server
		nsipcbuf.pkt.jp_len = len;   
		memmove(nsipcbuf.pkt.jp_data, buf, len);
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U);
		//sys_yield();
	}
}
