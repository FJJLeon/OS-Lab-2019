#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet request (using ipc_recv)
	//	- send the packet to the device driver (using sys_net_send)
	//	do the above things in a loop
	uint32_t req, whom;
	int perm, r;
	while (true) {
		// recv req from core-ns env
		req = ipc_recv((int32_t *) &whom, &nsipcbuf, &perm);
		if (req != NSREQ_OUTPUT) {
			cprintf("output env: ipc recv not NSREQ_OUTPUT\n");
			continue;
		}
		// syscall send pkt, if queue full, yield or loop
		while ((r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0) {
			if (r != -E_AGAIN)
				panic("output env: send fail but not full\n");
			cprintf("output env: net send queue full\n");
			sys_yield();
		}
	}
}
