#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xa8c16cf3, "module_layout" },
	{ 0xc1216b45, "xfrm4_tunnel_deregister" },
	{ 0xae24b68d, "unregister_pernet_device" },
	{ 0xab4e34b5, "xfrm4_tunnel_register" },
	{ 0x75a1ef0d, "register_pernet_device" },
	{ 0x609f1c7e, "synchronize_net" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0xf2e6372d, "netdev_state_change" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xc6cbbc89, "capable" },
	{ 0x125f1b98, "__ip_select_ident" },
	{ 0xb36c7dda, "icmp_send" },
	{ 0xd98b3288, "ip_local_out" },
	{ 0x68093a17, "skb_push" },
	{ 0x1038b781, "sock_wfree" },
	{ 0xd14d1b5, "skb_realloc_headroom" },
	{ 0xb5bd70d4, "consume_skb" },
	{ 0xd2c2ae7e, "register_netdev" },
	{ 0x2b534143, "__secpath_destroy" },
	{ 0x928b4b18, "__xfrm_policy_check" },
	{ 0x5d61b0b4, "netif_rx" },
	{ 0x1b6314fd, "in_aton" },
	{ 0x7628f3c7, "this_cpu_off" },
	{ 0x6e720ff2, "rtnl_unlock" },
	{ 0x9fdecc31, "unregister_netdevice_many" },
	{ 0x4cc9c5fb, "unregister_netdevice_queue" },
	{ 0xc7a4fbed, "rtnl_lock" },
	{ 0xe914e41e, "strcpy" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x4263b2df, "register_netdevice" },
	{ 0x949f7342, "__alloc_percpu" },
	{ 0x5792f848, "strlcpy" },
	{ 0x4befd54d, "alloc_netdev_mqs" },
	{ 0xdd939904, "__dev_get_by_index" },
	{ 0xe0668750, "dst_release" },
	{ 0x58910fc0, "ip_route_output_flow" },
	{ 0x7d11c268, "jiffies" },
	{ 0x7a0cf1f9, "tcp_parse_options" },
	{ 0x783c7933, "kmem_cache_alloc_trace" },
	{ 0x352091e6, "kmalloc_caches" },
	{ 0x37a0cba, "kfree" },
	{ 0x6ad0f196, "kfree_skb" },
	{ 0x2a18c74, "nf_conntrack_destroy" },
	{ 0xfe7c4287, "nr_cpu_ids" },
	{ 0xc0a3d105, "find_next_bit" },
	{ 0x3928efe9, "__per_cpu_offset" },
	{ 0xb9249d16, "cpu_possible_mask" },
	{ 0x357311d5, "free_netdev" },
	{ 0xc9ec4e21, "free_percpu" },
	{ 0x27e1a049, "printk" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=tunnel4";


MODULE_INFO(srcversion, "8ADA94FF81275A2076336A3");
