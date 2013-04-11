#ifndef _LINUX_NET_IPIP_STAGER
#define _LINUX_NET_IPIP_STAGER

#include <linux/inet.h>

struct connection_pair {
    __u16 src_port;
    __u16 dst_port;
};

static struct connection_pair iface_connections_eth0[50];
static struct connection_pair iface_connections_eth1[50];

static unsigned int iface_conn_count_eth0 = 0;
static unsigned int iface_conn_count_eth1 = 0;

static void add_connection_pair_to_iface(struct sk_buff *skb, int iface);
static int  get_iface_for_connection(struct sk_buff *skb);

#endif  /* _LINUX_INET_H */