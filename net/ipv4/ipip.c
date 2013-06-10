/*
 *  Linux NET3: IP/IP protocol decoder.
 *
 *  Authors:
 *      Sam Lantinga (slouken@cs.ucdavis.edu)  02/01/95
 *
 *  Fixes:
 *      Alan Cox    :   Merged and made usable non modular (its so tiny its silly as
 *                  a module taking up 2 pages).
 *      Alan Cox    :   Fixed bug with 1.3.18 and IPIP not working (now needs to set skb->h.iph)
 *                  to keep ip_forward happy.
 *      Alan Cox    :   More fixes for 1.3.21, and firewall fix. Maybe this will work soon 8).
 *      Kai Schulte :   Fixed #defines for IP_FIREWALL->FIREWALL
 *              David Woodhouse :       Perform some basic ICMP handling.
 *                                      IPIP Routing without decapsulation.
 *              Carlos Picoto   :       GRE over IP support
 *      Alexey Kuznetsov:   Reworked. Really, now it is truncated version of ipv4/ip_gre.c.
 *                  I do not want to merge them together.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

/* tunnel.c: an IP tunnel driver

    The purpose of this driver is to provide an IP tunnel through
    which you can tunnel network traffic transparently across subnets.

    This was written by looking at Nick Holloway's dummy driver
    Thanks for the great code!

        -Sam Lantinga   (slouken@cs.ucdavis.edu)  02/01/95

    Minor tweaks:
        Cleaned up the code a little and added some pre-1.3.0 tweaks.
        dev->hard_header/hard_header_len changed to use no headers.
        Comments/bracketing tweaked.
        Made the tunnels use dev->name not tunnel: when error reporting.
        Added tx_dropped stat

        -Alan Cox   (alan@lxorguk.ukuu.org.uk) 21 March 95

    Reworked:
        Changed to tunnel to destination gateway in addition to the
            tunnel's pointopoint address
        Almost completely rewritten
        Note:  There is currently no firewall or ICMP handling done.

        -Sam Lantinga   (slouken@cs.ucdavis.edu) 02/13/96

*/

/* Things I wish I had known when writing the tunnel driver:

    When the tunnel_xmit() function is called, the skb contains the
    packet to be sent (plus a great deal of extra info), and dev
    contains the tunnel device that _we_ are.

    When we are passed a packet, we are expected to fill in the
    source address with our source IP address.

    What is the proper way to allocate, copy and free a buffer?
    After you allocate it, it is a "0 length" chunk of memory
    starting at zero.  If you want to add headers to the buffer
    later, you'll have to call "skb_reserve(skb, amount)" with
    the amount of memory you want reserved.  Then, you call
    "skb_put(skb, amount)" with the amount of space you want in
    the buffer.  skb_put() returns a pointer to the top (#0) of
    that buffer.  skb->len is set to the amount of space you have
    "allocated" with skb_put().  You can then write up to skb->len
    bytes to that buffer.  If you need more, you can call skb_put()
    again with the additional amount of space you need.  You can
    find out how much more space you can allocate by calling
    "skb_tailroom(skb)".
    Now, to add header space, call "skb_push(skb, header_len)".
    This creates space at the beginning of the buffer and returns
    a pointer to this new space.  If later you need to strip a
    header from a buffer, call "skb_pull(skb, header_len)".
    skb_headroom() will return how much space is left at the top
    of the buffer (before the main data).  Remember, this headroom
    space must be reserved before the skb_put() function is called.
    */

/*
   This version of net/ipv4/ipip.c is cloned of net/ipv4/ip_gre.c

   For comments look at net/ipv4/ip_gre.c --ANK
 */


#include <linux/capability.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/mroute.h>
#include <linux/init.h>
#include <linux/netfilter_ipv4.h>
#include <linux/if_ether.h>

/* stager dev - need the following header for in_aton() */
#include <linux/inet.h>
#include <net/tcp.h> /* for tcp_parse_options() */
/* FIXME: move all stager-related code to a separate source */
// #include "ipip_stager.h"
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/list.h>



#include <net/sock.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/ipip.h>
#include <net/inet_ecn.h>
#include <net/xfrm.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

#define HASH_SIZE  16
#define HASH(addr) (((__force u32)addr^((__force u32)addr>>4))&0xF)


/* Stager Dev */
/* FIXME:
    * lists with connections count should not be hardcoded
    * lists with connections/conn count should be actively managed
    * replace list with connections to be a hash table
    * concurrency control on all global variables with RCU and locks
*/
#define PATHS_COUNT 2
#define TRACKER_INTERVAL 1950

static DEFINE_SPINLOCK(lock_path_flows_count);
static DEFINE_SPINLOCK(lock_update_flow);
static DEFINE_SPINLOCK(lock_add_flow);


struct l4_flow {
    __u16 src_port;
    __u16 dst_port;
    __u32 last_jiffies;
    __be32 bytes;
    u8 path_id;
    struct list_head flows_list;
};

struct old_l4_flow {
    struct l4_flow *old_flow;
    struct rcu_head rcu;
};

/* Packet descriptor: FIXME not used yet */
struct packet_descriptor {
    __be32 id;

    enum {
        STAGER_PACKET_TYPE_STICKY = 1,
        STAGER_PACKET_TYPE_ELASTIC = 0,
    } type:1;
};


/*** Stager state variables
    ***/

/* Which are the flows served on each path */
static LIST_HEAD(flows_head);

/* How many flows each path is serving */
static u16 path_flows_count[PATHS_COUNT] = {0, 0};

/* smoothed rtt (SRTT) */
static u16 srtt[PATHS_COUNT] = {0, 0};

/* SRTT remainder */
static u16 srtt_rest[PATHS_COUNT] = {0, 0};

/* minimum srtt */
static u16 srtt_min[PATHS_COUNT] = {USHRT_MAX, USHRT_MAX};

/* value of the srtt when the last restage was performed */
static u16 last_restage_srtt_val[PATHS_COUNT] = {USHRT_MAX, USHRT_MAX};

/* congestion factor */
static u16 cf[PATHS_COUNT] = {0, 0};



/*
    Should update the RTT average value per iface

    Called from ipip_rcv() -> add_l4_flow_to_iface()
*/
static void
_update_iface_stats(struct sk_buff *skb, int iface)
{
    const u8 *hash_location; /* we'll ignore this anyway */
    struct tcp_options_received *rx_opt; /* defined in linux/tcp.h */
    u16 *this_srtt, *this_srtt_min, *this_cf, *this_srtt_rest;
    u64 temp_srtt;
    u32 rtt_instant;
    s32 rtt_diff;
    struct tcphdr *tcp_header = tcp_hdr(skb);

    /* RST packets should not carry timestamps (rfc1323) */
    if (tcp_header->rst)
        return;

    /* Structure in whil we'll keep the TCP Header Options */
    rx_opt = (struct tcp_options_received*)
        kmalloc(sizeof(struct tcp_options_received), GFP_ATOMIC);

    tcp_parse_options(skb, rx_opt, &hash_location, 0);

    /* No need to bother with any updates if we can't compute the RTT */
    if (rx_opt->rcv_tsecr == 0 || rx_opt->rcv_tsecr > tcp_time_stamp)
        return;

    /* Get the last RTT */
    rtt_instant = tcp_time_stamp - rx_opt->rcv_tsecr;

    if (rtt_instant > 250)
        return;

    rcu_assign_pointer(this_srtt, &srtt[iface]);
    rcu_assign_pointer(this_srtt_min, &srtt_min[iface]);
    rcu_assign_pointer(this_cf, &cf[iface]);
    rcu_assign_pointer(this_srtt_rest, &srtt_rest[iface]);


    if (*this_srtt == 0){
        *this_srtt = rtt_instant;
    } else {
        /* Compute the average RTT */
        temp_srtt = 975*(*this_srtt)*1000 + 975*(*this_srtt_rest) + 25*rtt_instant*1000;
        temp_srtt = temp_srtt / 1000;
        *this_srtt = temp_srtt / 1000;
        *this_srtt_rest = temp_srtt % 1000;
    }

    /* FIXME: we should not depend on this hardcoded value */
    if (*this_srtt < *this_srtt_min && *this_srtt > 40)
        *this_srtt_min = *this_srtt;

    /* difference between the instant RTT and the smoothed RTT */
    // rtt_diff = rtt_instant - *this_srtt;
    // if (rtt_diff < 0) rtt_diff = 0;

    rtt_diff = *this_srtt - *this_srtt_min;
    if (rtt_diff < 0) rtt_diff = 0;

    /* Adjust the congestion factor; smooth variation */
    *this_cf = 9*(*this_cf) + 1*rtt_diff;
    *this_cf = *this_srtt / 10;


    printk(KERN_INFO "[stager][_update_iface_stats] Final stats for iface %d "
        "[rtt] %d [srtt] %d %d [srtt_min] %d %d [rttd] %d [rest] %d %d\n",
        iface, rtt_instant, srtt[0], srtt[1], srtt_min[0], srtt_min[1],
        rtt_diff, srtt_rest[0], srtt_rest[1]);
}


static void
flow_reclaim(struct rcu_head *rp)
{
    struct old_l4_flow *fp = container_of(rp, struct old_l4_flow, rcu);

    kfree(fp->old_flow);
    kfree(fp);
}

static void
_do_flow_add(__u16 isrc_port, __u16 idst_port, __be16 ibytes, int iface)
{
    u8 found = 0;
    struct l4_flow *p;
    __u32 report_timestamp = 0, delta_time = 0;
    __be32 report_bytes = 0;


    if (iface >= 2) return;

    /* Sequential search; check if we've already seen this flow */
    rcu_read_lock_bh();
    list_for_each_entry_rcu(p, &flows_head, flows_list)
    {

        u8 switched = 0;
        struct l4_flow *u_flow;
        struct old_l4_flow *rel_flow;

        spin_lock_bh(&lock_update_flow);
        if ((p->src_port == isrc_port) && (p->dst_port == idst_port))
        {
            delta_time =
                jiffies_to_msecs(tcp_time_stamp - p->last_jiffies);

            found = 1;

            u_flow = kmalloc(sizeof(struct l4_flow), GFP_ATOMIC);
            *u_flow = *p;

            rel_flow = kmalloc(sizeof(struct old_l4_flow), GFP_ATOMIC);
            rel_flow->old_flow = p;

            if (delta_time > TRACKER_INTERVAL)
            {
                report_timestamp = p->last_jiffies;
                report_bytes = p->bytes + ibytes;

                u_flow->last_jiffies = tcp_time_stamp;
                u_flow->bytes = 0;
            } else {
                u_flow->bytes += ibytes;
            }

            if (u_flow->path_id != iface)
            {
                u_flow->path_id = iface;
                switched = 1;
            }
            list_replace_rcu(&p->flows_list, &u_flow->flows_list);
            spin_unlock_bh(&lock_update_flow);
            call_rcu_bh(&rel_flow->rcu, flow_reclaim);

            /* Update the counter for flows per path */
            if (switched)
            {
                spin_lock_bh(&lock_path_flows_count);
                path_flows_count[1-iface]--;
                path_flows_count[iface]++;
                spin_unlock_bh(&lock_path_flows_count);
            }

            break;
        } else {
            spin_unlock_bh(&lock_update_flow);
        }
    }
    rcu_read_unlock_bh();

    /* Fresh new flow, allocate and add it to our list */
    if (found == 0)
    {
        struct l4_flow *flow_id;

        spin_lock_bh(&lock_add_flow);
        flow_id = kmalloc(sizeof(struct l4_flow), GFP_ATOMIC);

        flow_id->src_port = isrc_port;
        flow_id->dst_port = idst_port;
        flow_id->bytes = ibytes;
        flow_id->path_id = iface;
        flow_id->last_jiffies = tcp_time_stamp;

        list_add_rcu(&flow_id->flows_list, &flows_head);
        spin_unlock_bh(&lock_add_flow);

        spin_lock_bh(&lock_path_flows_count);
        path_flows_count[iface]++;
        spin_unlock_bh(&lock_path_flows_count);
    }

#ifndef STAGER_SITE_A
    if (report_timestamp > 0)
        printk(KERN_INFO "[stager][tracker][ %u > %u ] Iface: %d Bytes: %u\n",
                    isrc_port, idst_port,
                    iface,
                    (report_bytes*1000)/delta_time);
#endif
}


/* iface can be:
    * 0 -> add the flow to path_flows_count[0]
    * 1 -> add the flow to path_flows_count[1]

    Called from ipip_rcv()
*/
static void
add_l4_flow_to_iface(struct sk_buff *skb, int iface)
{
    struct iphdr *_ipheader = ip_hdr(skb);

    // printk(KERN_INFO " * add_l4_flow_to_iface\n");

    skb->transport_header = skb->network_header + ((ip_hdr(skb)->ihl)<<2);

    if (_ipheader->protocol == IPPROTO_TCP){
        struct tcphdr *tcp_header = tcp_hdr(skb);
        __u16 src, dst;
        __be16 bytes;

        _update_iface_stats(skb, iface);

        // printk(KERN_INFO "[add_l4_flow_to_iface] Port src: [%u]; dst: [%u]; seq: [%u]\n",
        //     ntohs(tcp_header->source), ntohs(tcp_header->dest), ntohl(tcp_header->seq));

        src = ntohs(tcp_header->source);
        dst = ntohs(tcp_header->dest);
        bytes = _ipheader->tot_len;

        _do_flow_add(src, dst, bytes, iface);

    } else {
        printk(KERN_INFO "[ipip_rcv] Protocol is not TCP\n");
        if (_ipheader->protocol == IPPROTO_ICMP){
            printk(KERN_INFO "[ipip_rcv] Protocol is ICMP\n");
        }
    }
}


static void
_do_flows_switch(u16 count, u16 from, u16 to)
{
    u16 still_switching = count;
    struct l4_flow *p;

    // printk(KERN_INFO "[stager] About to move %d flows from %d to %d\n",
    //     count, from, to);

    rcu_read_lock();
    list_for_each_entry_rcu(p, &flows_head, flows_list)
    {

        struct l4_flow *u_flow;
        struct old_l4_flow *rel_flow;

        spin_lock_bh(&lock_update_flow);
        if (p->path_id == from)
        {
            still_switching--;

            u_flow = kmalloc(sizeof(struct l4_flow), GFP_ATOMIC);
            *u_flow = *p;

            u_flow->path_id = to;

            rel_flow = kmalloc(sizeof(struct old_l4_flow), GFP_ATOMIC);
            rel_flow->old_flow = p;

            list_replace_rcu(&p->flows_list, &u_flow->flows_list);
            spin_unlock_bh(&lock_update_flow);
            call_rcu_bh(&rel_flow->rcu, flow_reclaim);

            /* Update the counter for flows per path */
            spin_lock_bh(&lock_path_flows_count);
            path_flows_count[from]--;
            path_flows_count[to]++;
            spin_unlock_bh(&lock_path_flows_count);

            if (still_switching == 0)
                break;

        } else {
            spin_unlock_bh(&lock_update_flow);
        }
    }
    rcu_read_unlock();
}


/* Restages the flows along the possible paths.

    Called from:
    ipip_tunnel_xmit() -> get_iface_for_skb() -> _stage_flows()
 */
static void
_do_restage(u16 *ideal_fc)
{
    u16 fc_diff, from_iface, to_iface;

    /* iface 0 should ideally have more connections on it */
    spin_lock_bh(&lock_path_flows_count);
    if (ideal_fc[0] > path_flows_count[0]){
        fc_diff = ideal_fc[0] - path_flows_count[0];
        spin_unlock_bh(&lock_path_flows_count);
        printk(KERN_INFO "[stager] Need to move %d flows from eth1 to eth0\n", fc_diff);
        from_iface = 1;
        to_iface = 0;
    } else {
        fc_diff = ideal_fc[1] - path_flows_count[1];
        spin_unlock_bh(&lock_path_flows_count);
        printk(KERN_INFO "[stager] Need to move %d flows from eth0 to eth1\n", fc_diff);
        from_iface = 0;
        to_iface = 1;
    }

    // if (fc_diff <= 2){
    //     return;
    // }

    // fc_diff = fc_diff / 2;

    /* Static route simulation: comment the following line */
    _do_flows_switch(fc_diff, from_iface, to_iface);
}


/* We'll restage the flows on all ifaces when both of the following conditions
    are true (for any of the ifaces):
        o srtt - (srtt_min) > 0.3 * srtt_min
        o |last_restage_srtt_val  - srtt| > 0.5 * srtt_min

    Alternative implementation - restage if the following is true:
        o for any of the ifaces there's a positive fluctuation in the SRTT
            calculation for a pre-defined number of steps

    Called from ipip_tunnel_xmit() -> get_iface_for_skb()
*/
static void
_stage_flows(void)
{
    u8 should_restage = 0;
    u8 i = 0;

    /* traffic weight, expressed in % of 100 */
    /* FIXME: remove static initialization from throughout the code */
    static u16 tw[PATHS_COUNT] = {0, 0};

    /* Establish if we'll need to restage */
    for (i = 0; i < PATHS_COUNT; ++i)
    {
        if ((10*(srtt[i] - (srtt_min[i])) > 5*srtt_min[i]) &&
            (10*abs(last_restage_srtt_val[i] - srtt[i]) > 5*srtt_min[i]))
        {
            // if (last_ts_switch == 0){
            //     last_ts_switch = tcp_time_stamp;
            //     should_restage = 1;
            // } else {
            //     if (tcp_time_stamp - last_ts_switch > 1000) {
            //         last_ts_switch = tcp_time_stamp;
            //         should_restage = 1;
            //     }
            // }
            should_restage = 1;

            // printk(KERN_INFO "[stager] Restage for iface: %d\n", i);
            last_restage_srtt_val[i] = srtt[i];
            break;
        }
    }

    // printk(KERN_INFO "Restage not needed.");

    /* Should perform some route calculation then re-staging here */
    if (should_restage)
    {
        int j = 0;
        int rest = 0;

        /* Congestion factor total: sum of cf from all paths */
        u16 cf_total = 0;

        /* Traffic count total: how many flows are being carried at the moment
            on all paths.
         */
        u16 tc_total;

        /* Ideal flow count: depending on the traffic weight per each path, how
            many flows should a path carry - relative to the total amount of
            flows on all paths (tc_total).
         */
        u16 ideal_fc[PATHS_COUNT];

        for (j = 0; j < PATHS_COUNT; ++j)
        {
            cf_total += cf[j];
        }

        /* Compute the traffic weight: how much weight is each iface carying atm.
           The tw (traffic weight) is relative to the congestion factor
        */
        if (cf_total != 0){
            for (j = 0; j < PATHS_COUNT; ++j)
            {
                tw[j] = 100 - ((100*(cf[j]))/cf_total);
            }
        }

        /* FIXME: how to distribute the flows ? */
        spin_lock_bh(&lock_path_flows_count);
        tc_total = path_flows_count[0] + path_flows_count[1];
        spin_unlock_bh(&lock_path_flows_count);

        ideal_fc[0] = (tc_total * tw[0] )/100;
        ideal_fc[0] = (ideal_fc[0] > 0) ? ideal_fc[0] : 1;

        rest = (tc_total*tw[0]) % 100;
        if (rest >= 50)
            ideal_fc[0]++;

        ideal_fc[1] = tc_total - ideal_fc[0];
        if ((tc_total > 1) && (ideal_fc[1] == 0)){
            ideal_fc[1] = 1;
            ideal_fc[0]--;
        }

        if (ideal_fc[0] != path_flows_count[0]){
            _do_restage(ideal_fc);
        }

        printk(KERN_INFO
            "[stager] tc_total %d fc %d %d tc %d %d tw %d %d; last srtt %d %d\n",
            tc_total,
            path_flows_count[0], path_flows_count[1],
            ideal_fc[0], ideal_fc[1],
            tw[0], tw[1],
            last_restage_srtt_val[0], last_restage_srtt_val[1]);
    }
}


/*
    Called from ipip_tunnel_xmit()
*/
static int
get_iface_for_skb(struct sk_buff *skb)
{
    struct tcphdr *tcp_header;
    __be16 source_port;
    __be16 dest_port;
    int result = 0;
    struct l4_flow *p;

#ifdef STAGER_SITE_A
    _stage_flows();
#endif

    skb->transport_header = skb->network_header + ((ip_hdr(skb)->ihl)<<2);
    tcp_header = tcp_hdr(skb);
    source_port = ntohs(tcp_header->source);
    dest_port = ntohs(tcp_header->dest);


#ifdef STAGER_SITE_A
    /* Temporary fix to have at least 1 flow on eth1 */
    if( (dest_port & 0x01) == 0 && (srtt_min[1] == USHRT_MAX) ){

    /* Static route simulation: even-numbered ports routed through iface 1 */
    // if ((dest_port & 0x01) == 0){
        // printk(KERN_INFO
        //     "[get_iface_for_skb] Traffic on port even-number [%d] is routed through iface 1.\n",
        //         dest_port);
        return 1;
    }
    // else {
    //     return 0;
    // }
#endif

    rcu_read_lock();
    list_for_each_entry_rcu(p, &flows_head, flows_list)
    {
        if ((p->src_port == dest_port) && (p->dst_port == source_port))
        {
            result = p->path_id;
            break;
        }
    }
    rcu_read_unlock();


    // printk(KERN_INFO "[get_iface_for_skb] flow [%d, %d] uses %d.\n",
    //     source_port, dest_port, result);
    return result;
}

/* Stager Dev */


static int ipip_net_id __read_mostly;
struct ipip_net {
    struct ip_tunnel __rcu *tunnels_r_l[HASH_SIZE];
    struct ip_tunnel __rcu *tunnels_r[HASH_SIZE];
    struct ip_tunnel __rcu *tunnels_l[HASH_SIZE];
    struct ip_tunnel __rcu *tunnels_wc[1];
    struct ip_tunnel __rcu **tunnels[4];

    struct net_device *fb_tunnel_dev;
};

static int ipip_tunnel_init(struct net_device *dev);
static void ipip_tunnel_setup(struct net_device *dev);
static void ipip_dev_free(struct net_device *dev);

/*
 * Locking : hash tables are protected by RCU and RTNL
 */

#define for_each_ip_tunnel_rcu(start) \
    for (t = rcu_dereference(start); t; t = rcu_dereference(t->next))

/* often modified stats are per cpu, other are shared (netdev->stats) */
struct pcpu_tstats {
    u64 rx_packets;
    u64 rx_bytes;
    u64 tx_packets;
    u64 tx_bytes;
    struct u64_stats_sync   syncp;
};

static struct rtnl_link_stats64 *ipip_get_stats64(struct net_device *dev,
                          struct rtnl_link_stats64 *tot)
{
    int i;

    /* stager dev */
    // printk(KERN_INFO "* ipip_get_stats64\n");


    for_each_possible_cpu(i) {
        const struct pcpu_tstats *tstats = per_cpu_ptr(dev->tstats, i);
        u64 rx_packets, rx_bytes, tx_packets, tx_bytes;
        unsigned int start;

        do {
            start = u64_stats_fetch_begin_bh(&tstats->syncp);
            rx_packets = tstats->rx_packets;
            tx_packets = tstats->tx_packets;
            rx_bytes = tstats->rx_bytes;
            tx_bytes = tstats->tx_bytes;
        } while (u64_stats_fetch_retry_bh(&tstats->syncp, start));

        tot->rx_packets += rx_packets;
        tot->tx_packets += tx_packets;
        tot->rx_bytes   += rx_bytes;
        tot->tx_bytes   += tx_bytes;
    }

    tot->tx_fifo_errors = dev->stats.tx_fifo_errors;
    tot->tx_carrier_errors = dev->stats.tx_carrier_errors;
    tot->tx_dropped = dev->stats.tx_dropped;
    tot->tx_aborted_errors = dev->stats.tx_aborted_errors;
    tot->tx_errors = dev->stats.tx_errors;
    tot->collisions = dev->stats.collisions;

    return tot;
}

static struct ip_tunnel *ipip_tunnel_lookup(struct net *net,
        __be32 remote, __be32 local)
{
    unsigned int h0 = HASH(remote);
    unsigned int h1 = HASH(local);
    struct ip_tunnel *t;
    struct ipip_net *ipn = net_generic(net, ipip_net_id);

    for_each_ip_tunnel_rcu(ipn->tunnels_r_l[h0 ^ h1])
        if (local == t->parms.iph.saddr &&
            remote == t->parms.iph.daddr && (t->dev->flags&IFF_UP))
            return t;

    for_each_ip_tunnel_rcu(ipn->tunnels_r[h0])
        if (remote == t->parms.iph.daddr && (t->dev->flags&IFF_UP))
            return t;

    for_each_ip_tunnel_rcu(ipn->tunnels_l[h1])
        if (local == t->parms.iph.saddr && (t->dev->flags&IFF_UP))
            return t;

    t = rcu_dereference(ipn->tunnels_wc[0]);
    if (t && (t->dev->flags&IFF_UP))
        return t;

    /* stager dev */
    /* override the lookup procedure and return our only tunnel */
    // printk(KERN_INFO "    overriding lookup\n");
    return rcu_dereference(ipn->tunnels_r_l[h0 ^ h1]);

    return NULL;
}

static struct ip_tunnel __rcu **__ipip_bucket(struct ipip_net *ipn,
        struct ip_tunnel_parm *parms)
{
    __be32 remote = parms->iph.daddr;
    __be32 local = parms->iph.saddr;
    unsigned int h = 0;
    int prio = 0;

    /* stager dev */
    // printk(KERN_INFO "* __ipip_bucket\n");

    if (remote) {
        prio |= 2;
        h ^= HASH(remote);
    }
    if (local) {
        prio |= 1;
        h ^= HASH(local);
    }

    /* stager dev */
    // printk(KERN_INFO "    remote: [%pI4]; local: [%pI4]; prio: [%d]; hash: [%u]\n",
    //     &remote, &local, prio, h);

    return &ipn->tunnels[prio][h];
}

static inline struct ip_tunnel __rcu **ipip_bucket(struct ipip_net *ipn,
        struct ip_tunnel *t)
{
    return __ipip_bucket(ipn, &t->parms);
}

static void ipip_tunnel_unlink(struct ipip_net *ipn, struct ip_tunnel *t)
{
    struct ip_tunnel __rcu **tp;
    struct ip_tunnel *iter;

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_unlink\n");

    for (tp = ipip_bucket(ipn, t);
         (iter = rtnl_dereference(*tp)) != NULL;
         tp = &iter->next) {
        if (t == iter) {
            rcu_assign_pointer(*tp, t->next);
            break;
        }
    }
}


static void ipip_tunnel_link(struct ipip_net *ipn, struct ip_tunnel *t)
{
    struct ip_tunnel __rcu **tp = ipip_bucket(ipn, t);

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_link\n");


    rcu_assign_pointer(t->next, rtnl_dereference(*tp));
    rcu_assign_pointer(*tp, t);
}

static struct ip_tunnel *ipip_tunnel_locate(struct net *net,
        struct ip_tunnel_parm *parms, int create)
{
    __be32 remote = parms->iph.daddr;
    __be32 local = parms->iph.saddr;
    struct ip_tunnel *t, *nt;
    struct ip_tunnel __rcu **tp;
    struct net_device *dev;
    char name[IFNAMSIZ];
    struct ipip_net *ipn = net_generic(net, ipip_net_id);

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_locate\n");
    // printk(KERN_INFO "    create: [%d]; name: [%s]; remote: [%pI4]; local: [%pI4]\n",
    //     create, parms->name, &remote, &local);

    for (tp = __ipip_bucket(ipn, parms);
         (t = rtnl_dereference(*tp)) != NULL;
         tp = &t->next) {
        if (local == t->parms.iph.saddr && remote == t->parms.iph.daddr)
            return t;
    }
    if (!create)
        return NULL;

    if (parms->name[0])
        strlcpy(name, parms->name, IFNAMSIZ);
    else
        strcpy(name, "tunl%d");


    /* stager dev */
    /*
        The following call to alloc_netdev() will trigger a call to:
        ipip_tunnel_setup()
     */

    dev = alloc_netdev(sizeof(*t), name, ipip_tunnel_setup);
    if (dev == NULL)
        return NULL;

    /* stager dev */
    /* Link the net namespace with the dev */
    dev_net_set(dev, net);

    /* stager dev */
    /* Save the parms in the dev private space */
    nt = netdev_priv(dev);
    nt->parms = *parms;


    /* stager dev */
    /*
        ipip_tunnel_init will call:
            ipip_tunnel_bind_dev
     */

    if (ipip_tunnel_init(dev) < 0)
        goto failed_free;

    if (register_netdevice(dev) < 0)
        goto failed_free;

    strcpy(nt->parms.name, dev->name);

    dev_hold(dev);
    ipip_tunnel_link(ipn, nt);
    return nt;

failed_free:
    ipip_dev_free(dev);
    return NULL;
}

/* called with RTNL */
static void ipip_tunnel_uninit(struct net_device *dev)
{
    struct net *net = dev_net(dev);
    struct ipip_net *ipn = net_generic(net, ipip_net_id);

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_uninit\n");

    if (dev == ipn->fb_tunnel_dev)
        RCU_INIT_POINTER(ipn->tunnels_wc[0], NULL);
    else
        ipip_tunnel_unlink(ipn, netdev_priv(dev));
    dev_put(dev);
}

static int ipip_err(struct sk_buff *skb, u32 info)
{

/* All the routers (except for Linux) return only
   8 bytes of packet payload. It means, that precise relaying of
   ICMP in the real Internet is absolutely infeasible.
 */
    const struct iphdr *iph = (const struct iphdr *)skb->data;
    const int type = icmp_hdr(skb)->type;
    const int code = icmp_hdr(skb)->code;
    struct ip_tunnel *t;
    int err;

    /* stager dev */
    // printk(KERN_INFO "* ipip_err\n");

    switch (type) {
    default:
    case ICMP_PARAMETERPROB:
        return 0;

    case ICMP_DEST_UNREACH:
        switch (code) {
        case ICMP_SR_FAILED:
        case ICMP_PORT_UNREACH:
            /* Impossible event. */
            return 0;
        case ICMP_FRAG_NEEDED:
            /* Soft state for pmtu is maintained by IP core. */
            return 0;
        default:
            /* All others are translated to HOST_UNREACH.
               rfc2003 contains "deep thoughts" about NET_UNREACH,
               I believe they are just ether pollution. --ANK
             */
            break;
        }
        break;
    case ICMP_TIME_EXCEEDED:
        if (code != ICMP_EXC_TTL)
            return 0;
        break;
    }

    err = -ENOENT;

    rcu_read_lock();
    t = ipip_tunnel_lookup(dev_net(skb->dev), iph->daddr, iph->saddr);
    if (t == NULL || t->parms.iph.daddr == 0)
        goto out;

    err = 0;
    if (t->parms.iph.ttl == 0 && type == ICMP_TIME_EXCEEDED)
        goto out;

    if (time_before(jiffies, t->err_time + IPTUNNEL_ERR_TIMEO))
        t->err_count++;
    else
        t->err_count = 1;
    t->err_time = jiffies;
out:
    rcu_read_unlock();
    return err;
}

static inline void ipip_ecn_decapsulate(const struct iphdr *outer_iph,
                    struct sk_buff *skb)
{
    struct iphdr *inner_iph = ip_hdr(skb);

    /* stager dev */
    // printk(KERN_INFO "* ipip_ecn_decapsulate\n");

    if (INET_ECN_is_ce(outer_iph->tos))
        IP_ECN_set_ce(inner_iph);
}

static int ipip_rcv(struct sk_buff *skb)
{
    struct ip_tunnel *tunnel;
    const struct iphdr *iph = ip_hdr(skb);

    /* stager dev */
    // printk(KERN_INFO "* ipip_rcv\n");

    rcu_read_lock();
    tunnel = ipip_tunnel_lookup(dev_net(skb->dev), iph->saddr, iph->daddr);
    if (tunnel != NULL) {
        struct pcpu_tstats *tstats;

        if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb)) {
            rcu_read_unlock();
            kfree_skb(skb);
            return 0;
        }

        secpath_reset(skb);

        skb->mac_header = skb->network_header;
        skb_reset_network_header(skb);
        skb->protocol = htons(ETH_P_IP);
        skb->pkt_type = PACKET_HOST;

        tstats = this_cpu_ptr(tunnel->dev->tstats);
        u64_stats_update_begin(&tstats->syncp);
        tstats->rx_packets++;
        tstats->rx_bytes += skb->len;
        u64_stats_update_end(&tstats->syncp);

        /* stager dev */
        // printk(KERN_INFO "[ipip_rcv] outer: [%pI4, %pI4]; inner: [%pI4, %pI4]\n",
        //     &iph->saddr, &iph->daddr, &ip_hdr(skb)->saddr, &ip_hdr(skb)->daddr);
        if ((in_aton("192.168.0.2") == iph->daddr) ||
            (in_aton("192.168.1.2") == iph->daddr)) {
            add_l4_flow_to_iface(skb, 0);
        } else {
            add_l4_flow_to_iface(skb, 1);
        }

        __skb_tunnel_rx(skb, tunnel->dev);
        ipip_ecn_decapsulate(iph, skb);

        netif_rx(skb);

        rcu_read_unlock();
        return 0;
    }
    rcu_read_unlock();

    /* stager dev */
    // printk(KERN_INFO "    tunnel_lookup returned NULL && packet dropped\n");

    return -1;
}

/*
 *  This function assumes it is being called from dev_queue_xmit()
 *  and that skb is filled properly by that function.
 */

static netdev_tx_t ipip_tunnel_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ip_tunnel *tunnel = netdev_priv(dev);
    struct pcpu_tstats *tstats;
    const struct iphdr  *tiph = &tunnel->parms.iph;
    u8     tos = tunnel->parms.iph.tos;
    __be16 df = tiph->frag_off;
    struct rtable *rt;              /* Route to the other host */
    struct net_device *tdev;        /* Device to other host */
    const struct iphdr  *old_iph = ip_hdr(skb);
    struct iphdr  *iph;         /* Our new IP header */
    unsigned int max_headroom;      /* The extra header space needed */
    __be32 dst = tiph->daddr;
    struct flowi4 fl4;
    int    mtu;
    int i;

    /* stager dev */
    __be32 mangle_dst;
    __be32 mangle_src;
    // printk(KERN_INFO "* ipip_tunnel_xmit\n");


    if (skb->protocol != htons(ETH_P_IP))
        goto tx_error;

    if (tos & 1)
        tos = old_iph->tos;

    if (!dst) {
        /* NBMA tunnel */
        if ((rt = skb_rtable(skb)) == NULL) {
            dev->stats.tx_fifo_errors++;
            goto tx_error;
        }
        dst = rt->rt_gateway;
    }

    /* stager dev */
    /* change the header IP src and dst to be the hardcoded ones */
    if (old_iph->protocol == IPPROTO_TCP){
        i = get_iface_for_skb(skb);
    } else {
        printk(KERN_INFO "[ipip_tunnel_xmit] Not a TCP flow. Using default iface [0]\n");
        i = 0;
    }
    // printk(KERN_INFO "[ipip_tunnel_xmit] Sending packet on iface: eth%d.\n", i);

    /* Need to mangle if 1 */
    if (i == 1){
        /* We're on site A */
        if (tiph->saddr == in_aton("192.168.0.2")){
            mangle_dst = in_aton("192.168.3.2");
            mangle_src = in_aton("192.168.2.2");
        } else {
            /* We're on site B */
            mangle_dst = in_aton("192.168.2.2");
            mangle_src = in_aton("192.168.3.2");
        }
    } else {
        /* No need to mangle; by default we're sending on 0 */
        mangle_src = tiph->saddr;
        mangle_dst = dst;
    }

    rt = ip_route_output_ports(dev_net(dev), &fl4, NULL,
                   // dst, tiph->saddr,
                   mangle_dst, mangle_src,
                   0, 0,
                   IPPROTO_IPIP, RT_TOS(tos),
                   tunnel->parms.link);
    if (IS_ERR(rt)) {
        dev->stats.tx_carrier_errors++;
        goto tx_error_icmp;
    }
    tdev = rt->dst.dev;

    if (tdev == dev) {
        ip_rt_put(rt);
        dev->stats.collisions++;
        goto tx_error;
    }

    df |= old_iph->frag_off & htons(IP_DF);

    if (df) {
        mtu = dst_mtu(&rt->dst) - sizeof(struct iphdr);

        if (mtu < 68) {
            dev->stats.collisions++;
            ip_rt_put(rt);
            goto tx_error;
        }

        if (skb_dst(skb))
            skb_dst(skb)->ops->update_pmtu(skb_dst(skb), mtu);

        if ((old_iph->frag_off & htons(IP_DF)) &&
            mtu < ntohs(old_iph->tot_len)) {
            icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
                  htonl(mtu));
            ip_rt_put(rt);
            goto tx_error;
        }
    }

    if (tunnel->err_count > 0) {
        if (time_before(jiffies,
                tunnel->err_time + IPTUNNEL_ERR_TIMEO)) {
            tunnel->err_count--;
            dst_link_failure(skb);
        } else
            tunnel->err_count = 0;
    }

    /*
     * Okay, now see if we can stuff it in the buffer as-is.
     */
    max_headroom = (LL_RESERVED_SPACE(tdev)+sizeof(struct iphdr));

    if (skb_headroom(skb) < max_headroom || skb_shared(skb) ||
        (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
        struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
        if (!new_skb) {
            ip_rt_put(rt);
            dev->stats.tx_dropped++;
            dev_kfree_skb(skb);
            return NETDEV_TX_OK;
        }
        if (skb->sk)
            skb_set_owner_w(new_skb, skb->sk);
        dev_kfree_skb(skb);
        skb = new_skb;
        old_iph = ip_hdr(skb);
    }

    skb->transport_header = skb->network_header;
    skb_push(skb, sizeof(struct iphdr));
    skb_reset_network_header(skb);
    memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
    IPCB(skb)->flags &= ~(IPSKB_XFRM_TUNNEL_SIZE | IPSKB_XFRM_TRANSFORMED |
                  IPSKB_REROUTED);
    skb_dst_drop(skb);
    skb_dst_set(skb, &rt->dst);

    /*
     *  Push down and install the IPIP header.
     */

    iph             =   ip_hdr(skb);
    iph->version        =   4;
    iph->ihl        =   sizeof(struct iphdr)>>2;
    iph->frag_off       =   df;
    iph->protocol       =   IPPROTO_IPIP;
    iph->tos        =   INET_ECN_encapsulate(tos, old_iph->tos);
    iph->daddr      =   fl4.daddr;
    iph->saddr      =   fl4.saddr;

    if ((iph->ttl = tiph->ttl) == 0)
        iph->ttl    =   old_iph->ttl;

    nf_reset(skb);
    tstats = this_cpu_ptr(dev->tstats);
    __IPTUNNEL_XMIT(tstats, &dev->stats);
    return NETDEV_TX_OK;

tx_error_icmp:
    dst_link_failure(skb);
tx_error:
    dev->stats.tx_errors++;
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static void ipip_tunnel_bind_dev(struct net_device *dev)
{
    struct net_device *tdev = NULL;
    struct ip_tunnel *tunnel;
    const struct iphdr *iph;

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_bind_dev\n");

    tunnel = netdev_priv(dev);
    iph = &tunnel->parms.iph;

    if (iph->daddr) {
        struct rtable *rt;
        struct flowi4 fl4;

        rt = ip_route_output_ports(dev_net(dev), &fl4, NULL,
                       iph->daddr, iph->saddr,
                       0, 0,
                       IPPROTO_IPIP,
                       RT_TOS(iph->tos),
                       tunnel->parms.link);
        if (!IS_ERR(rt)) {
            tdev = rt->dst.dev;
            ip_rt_put(rt);
        }
        dev->flags |= IFF_POINTOPOINT;
    }

    if (!tdev && tunnel->parms.link)
        tdev = __dev_get_by_index(dev_net(dev), tunnel->parms.link);

    if (tdev) {
        dev->hard_header_len = tdev->hard_header_len + sizeof(struct iphdr);
        dev->mtu = tdev->mtu - sizeof(struct iphdr);
    }
    dev->iflink = tunnel->parms.link;
}

static int
ipip_tunnel_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd)
{
    int err = 0;
    struct ip_tunnel_parm p;
    struct ip_tunnel *t;
    struct net *net = dev_net(dev);
    struct ipip_net *ipn = net_generic(net, ipip_net_id);

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_ioctl\n");

    switch (cmd) {
    case SIOCGETTUNNEL:
        t = NULL;
        if (dev == ipn->fb_tunnel_dev) {
            if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
                err = -EFAULT;
                break;
            }
            t = ipip_tunnel_locate(net, &p, 0);
        }
        if (t == NULL)
            t = netdev_priv(dev);
        memcpy(&p, &t->parms, sizeof(p));
        if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof(p)))
            err = -EFAULT;
        break;

    case SIOCADDTUNNEL:
    case SIOCCHGTUNNEL:
        err = -EPERM;
        if (!capable(CAP_NET_ADMIN))
            goto done;

        err = -EFAULT;
        if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
            goto done;

        err = -EINVAL;
        if (p.iph.version != 4 || p.iph.protocol != IPPROTO_IPIP ||
            p.iph.ihl != 5 || (p.iph.frag_off&htons(~IP_DF)))
            goto done;
        if (p.iph.ttl)
            p.iph.frag_off |= htons(IP_DF);

        t = ipip_tunnel_locate(net, &p, cmd == SIOCADDTUNNEL);

        if (dev != ipn->fb_tunnel_dev && cmd == SIOCCHGTUNNEL) {
            if (t != NULL) {
                if (t->dev != dev) {
                    err = -EEXIST;
                    break;
                }
            } else {
                if (((dev->flags&IFF_POINTOPOINT) && !p.iph.daddr) ||
                    (!(dev->flags&IFF_POINTOPOINT) && p.iph.daddr)) {
                    err = -EINVAL;
                    break;
                }
                t = netdev_priv(dev);
                ipip_tunnel_unlink(ipn, t);
                synchronize_net();
                t->parms.iph.saddr = p.iph.saddr;
                t->parms.iph.daddr = p.iph.daddr;
                memcpy(dev->dev_addr, &p.iph.saddr, 4);
                memcpy(dev->broadcast, &p.iph.daddr, 4);
                ipip_tunnel_link(ipn, t);
                netdev_state_change(dev);
            }
        }

        if (t) {
            err = 0;
            if (cmd == SIOCCHGTUNNEL) {
                t->parms.iph.ttl = p.iph.ttl;
                t->parms.iph.tos = p.iph.tos;
                t->parms.iph.frag_off = p.iph.frag_off;
                if (t->parms.link != p.link) {
                    t->parms.link = p.link;
                    ipip_tunnel_bind_dev(dev);
                    netdev_state_change(dev);
                }
            }
            if (copy_to_user(ifr->ifr_ifru.ifru_data, &t->parms, sizeof(p)))
                err = -EFAULT;
        } else
            err = (cmd == SIOCADDTUNNEL ? -ENOBUFS : -ENOENT);
        break;

    case SIOCDELTUNNEL:
        err = -EPERM;
        if (!capable(CAP_NET_ADMIN))
            goto done;

        if (dev == ipn->fb_tunnel_dev) {
            err = -EFAULT;
            if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
                goto done;
            err = -ENOENT;
            if ((t = ipip_tunnel_locate(net, &p, 0)) == NULL)
                goto done;
            err = -EPERM;
            if (t->dev == ipn->fb_tunnel_dev)
                goto done;
            dev = t->dev;
        }
        unregister_netdevice(dev);
        err = 0;
        break;

    default:
        err = -EINVAL;
    }

done:
    return err;
}

static int ipip_tunnel_change_mtu(struct net_device *dev, int new_mtu)
{
    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_change_mtu\n");

    if (new_mtu < 68 || new_mtu > 0xFFF8 - sizeof(struct iphdr))
        return -EINVAL;
    dev->mtu = new_mtu;
    return 0;
}

static const struct net_device_ops ipip_netdev_ops = {
    .ndo_uninit = ipip_tunnel_uninit,
    .ndo_start_xmit = ipip_tunnel_xmit,
    .ndo_do_ioctl   = ipip_tunnel_ioctl,
    .ndo_change_mtu = ipip_tunnel_change_mtu,
    .ndo_get_stats64 = ipip_get_stats64,
};

static void ipip_dev_free(struct net_device *dev)
{
    /* stager dev */
    // printk(KERN_INFO "* ipip_dev_free\n");

    free_percpu(dev->tstats);
    free_netdev(dev);
}

static void ipip_tunnel_setup(struct net_device *dev)
{
    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_setup: filling the net_device structure\n");

    dev->netdev_ops     = &ipip_netdev_ops;
    dev->destructor     = ipip_dev_free;

    dev->type       = ARPHRD_TUNNEL;
    dev->hard_header_len    = LL_MAX_HEADER + sizeof(struct iphdr);
    dev->mtu        = ETH_DATA_LEN - sizeof(struct iphdr);
    dev->flags      = IFF_NOARP;
    dev->iflink     = 0;
    dev->addr_len       = 4;
    dev->features       |= NETIF_F_NETNS_LOCAL;
    dev->features       |= NETIF_F_LLTX;
    dev->priv_flags     &= ~IFF_XMIT_DST_RELEASE;
}

static int ipip_tunnel_init(struct net_device *dev)
{
    struct ip_tunnel *tunnel = netdev_priv(dev);

    /* stager dev */
    // printk(KERN_INFO "* ipip_tunnel_init\n");
    // printk(KERN_INFO "    saddr: [%pI4]; daddr: [%pI4]\n",
    //     &tunnel->parms.iph.saddr, &tunnel->parms.iph.daddr);

    tunnel->dev = dev;

    memcpy(dev->dev_addr, &tunnel->parms.iph.saddr, 4);
    memcpy(dev->broadcast, &tunnel->parms.iph.daddr, 4);

    ipip_tunnel_bind_dev(dev);

    dev->tstats = alloc_percpu(struct pcpu_tstats);
    if (!dev->tstats)
        return -ENOMEM;

    return 0;
}

static int __net_init ipip_fb_tunnel_init(struct net_device *dev)
{
    struct ip_tunnel *tunnel = netdev_priv(dev);
    struct iphdr *iph = &tunnel->parms.iph;
    struct ipip_net *ipn = net_generic(dev_net(dev), ipip_net_id);

    /* stager dev */
    // printk(KERN_INFO "* ipip_fb_tunnel_init\n");
    // printk(KERN_INFO "    dev->name: [%s]\n", dev->name);

    tunnel->dev = dev;
    strcpy(tunnel->parms.name, dev->name);

    iph->version        = 4;
    iph->protocol       = IPPROTO_IPIP;
    iph->ihl        = 5;

    dev->tstats = alloc_percpu(struct pcpu_tstats);
    if (!dev->tstats)
        return -ENOMEM;

    dev_hold(dev);
    rcu_assign_pointer(ipn->tunnels_wc[0], tunnel);
    return 0;
}

static struct xfrm_tunnel ipip_handler __read_mostly = {
    .handler    =   ipip_rcv,
    .err_handler    =   ipip_err,
    .priority   =   1,
};

static const char banner[] __initconst =
    KERN_INFO "IPv4 over IPv4 tunneling driver\n";

static void ipip_destroy_tunnels(struct ipip_net *ipn, struct list_head *head)
{
    int prio;

    /* stager dev */
    // printk(KERN_INFO "* ipip_destroy_tunnels\n");

    for (prio = 1; prio < 4; prio++) {
        int h;
        for (h = 0; h < HASH_SIZE; h++) {
            struct ip_tunnel *t;

            t = rtnl_dereference(ipn->tunnels[prio][h]);
            while (t != NULL) {
                unregister_netdevice_queue(t->dev, head);
                t = rtnl_dereference(t->next);
            }
        }
    }
}

static int __net_init ipip_init_net(struct net *net)
{
    struct ipip_net *ipn = net_generic(net, ipip_net_id);
    struct ip_tunnel *t;
    int err;

    /* stager dev */
    // printk(KERN_INFO "* ipip_init_net\n");

    ipn->tunnels[0] = ipn->tunnels_wc;
    ipn->tunnels[1] = ipn->tunnels_l;
    ipn->tunnels[2] = ipn->tunnels_r;
    ipn->tunnels[3] = ipn->tunnels_r_l;

    ipn->fb_tunnel_dev = alloc_netdev(sizeof(struct ip_tunnel),
                       "tunl0",
                       ipip_tunnel_setup);
    if (!ipn->fb_tunnel_dev) {
        err = -ENOMEM;
        goto err_alloc_dev;
    }
    dev_net_set(ipn->fb_tunnel_dev, net);

    err = ipip_fb_tunnel_init(ipn->fb_tunnel_dev);
    if (err)
        goto err_reg_dev;

    if ((err = register_netdev(ipn->fb_tunnel_dev)))
        goto err_reg_dev;

    t = netdev_priv(ipn->fb_tunnel_dev);

    strcpy(t->parms.name, ipn->fb_tunnel_dev->name);
    return 0;

err_reg_dev:
    ipip_dev_free(ipn->fb_tunnel_dev);
err_alloc_dev:
    /* nothing */
    return err;
}

static void __net_exit ipip_exit_net(struct net *net)
{
    struct ipip_net *ipn = net_generic(net, ipip_net_id);
    LIST_HEAD(list);

    /* stager dev */
    // printk(KERN_INFO "* ipip_exit_net\n");
    // printk(KERN_INFO "    net->proc_net->name: [%s]\n", net->proc_net->name);

    rtnl_lock();
    ipip_destroy_tunnels(ipn, &list);
    unregister_netdevice_queue(ipn->fb_tunnel_dev, &list);
    unregister_netdevice_many(&list);
    rtnl_unlock();
}

static struct pernet_operations ipip_net_ops = {
    .init = ipip_init_net,
    .exit = ipip_exit_net,
    .id   = &ipip_net_id,
    .size = sizeof(struct ipip_net),
};

static int __init ipip_init(void)
{
    int err;

    /* stager dev */
    // printk(KERN_INFO "* ipip_init\n");

    printk(banner);

    err = register_pernet_device(&ipip_net_ops);
    if (err < 0)
        return err;
    err = xfrm4_tunnel_register(&ipip_handler, AF_INET);
    if (err < 0) {
        unregister_pernet_device(&ipip_net_ops);
        pr_info("%s: can't register tunnel\n", __func__);
    }
    return err;
}

static void __exit ipip_fini(void)
{
    /* stager dev */
    // printk(KERN_INFO "* ipip_fini\n");

    if (xfrm4_tunnel_deregister(&ipip_handler, AF_INET))
        pr_info("%s: can't deregister tunnel\n", __func__);

    unregister_pernet_device(&ipip_net_ops);
}

module_init(ipip_init);
module_exit(ipip_fini);
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETDEV("tunl0");
