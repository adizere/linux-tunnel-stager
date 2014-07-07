#ifndef _LINUX_NET_IPIP_STAGER
#define _LINUX_NET_IPIP_STAGER

#include <linux/inet.h>

/* Identifies a Layer 4 flow */
struct l4_flow {
    __u16 src_port;
    __u16 dst_port;
};


static struct l4_flow iface_connections_eth0[50];
static struct l4_flow iface_connections_eth1[50];

static unsigned int iface_conn_count_eth0 = 0;
static unsigned int iface_conn_count_eth1 = 0;

void add_l4_flow_to_iface(struct sk_buff *skb, int iface);
int  get_iface_for_skb(struct sk_buff *skb);

#endif  /* _LINUX_NET_IPIP_STAGER */