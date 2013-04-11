#include <linux/tcp.h>

#include "ipip_stager.h"


static void
add_connection_pair_to_iface(struct sk_buff *skb, int iface)
{
	struct tcphdr *tcp_header = tcp_hdr(skb);
}