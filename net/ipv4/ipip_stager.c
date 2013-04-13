#include <linux/tcp.h>

#include "ipip_stager.h"


void
add_l4_flow_to_iface(struct sk_buff *skb, int iface)
{
	// struct tcphdr *tcp_header = tcp_hdr(skb);
	printk(KERN_INFO "inside stager\n");
}

int
get_iface_for_skb(struct sk_buff *skb)
{
	return 1;
}