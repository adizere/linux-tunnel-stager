linux-tunnel-stager
===================

Linux 3.5 L3 tunnel mangling tests.

On source site:
---------------
	make KCPPFLAGS=-DSTAGER_SITE_A
	modprobe tunnel4
	insmod ipip.ko
	bash scripts/ipip/crux-a-setup.sh


On destination site:
--------------------
	make
	modprobe tunnel4
	insmod ipip.ko
	bash scripts/ipip/crux-b-setup.sh