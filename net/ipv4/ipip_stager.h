#ifndef _LINUX_NET_IPIP_STAGER
#define _LINUX_NET_IPIP_STAGER

#include <linux/inet.h>


static unsigned int pairs_count = 2;

struct staged_pair {
    unsigned int net_device_iflink;
    __be32 daddr;
    __be32 saddr;
};


static void get_pairs_site_a(struct **pairs)
{
    // pairs = (struct staged_pair*);

    // pairs[0] = (struct staged_pair*)
    //     kmalloc(sizeof(struct staged_pair), GFP_KERNEL);
    // pairs[0]->net_device_iflink = 0;
    // pairs[0]->saddr = in_aton("192.168.0.2");
    // pairs[0]->daddr = in_aton("192.168.1.2");

    // pairs[1] = (struct staged_pair*)
    //     kmalloc(sizeof(struct staged_pair), GFP_KERNEL);
    // pairs[1]->net_device_iflink = 1;
    // pairs[1]->saddr = in_aton("192.168.2.2");
    // pairs[1]->daddr = in_aton("192.168.3.2");
}


static void get_pairs_site_b(struct **pairs)
{
//     struct staged_pair *pairs[pairs_count - 1];

//     pairs[0] = (struct staged_pair*)
//         kmalloc(sizeof(struct staged_pair), GFP_KERNEL);
//     pairs[0]->net_device_iflink = 0;
//     pairs[0]->saddr = in_aton("192.168.1.2");
//     pairs[0]->daddr = in_aton("192.168.0.2");

//     pairs[1] = (struct staged_pair*)
//         kmalloc(sizeof(struct staged_pair), GFP_KERNEL);
//     pairs[1]->net_device_iflink = 1;
//     pairs[1]->saddr = in_aton("192.168.3.2");
//     pairs[1]->daddr = in_aton("192.168.2.2");
}

EXPORT_SYMBOL(get_pairs_site_a);
EXPORT_SYMBOL(get_pairs_site_b);

#endif  /* _LINUX_INET_H */