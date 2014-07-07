#!/bin/bash

DEV=eth0
tc qdisc del dev $DEV root

tc qdisc add dev $DEV root handle 1:0 htb
tc class add dev $DEV parent 1:0 classid 1:1 htb rate 798.4Kbit burst 4.2kb cburst 4.2kb
tc qdisc add dev $DEV parent 1:1 handle 10:0 tbf rate 550kbit burst 5200 limit 5.2kb
#tc qdisc add dev $DEV parent 1:1 handle 10:0 pfifo limit 8

tc filter add dev $DEV parent 1:0 protocol ip prio 1 u32 match ip dport 5557 0xffff flowid 1:1
tc filter add dev $DEV parent 1:0 protocol ip prio 1 u32 match ip protocol 6 0xff match ip sport $1 0xffff flowid 1:1

tc -s -d qdisc show dev $DEV



tc qdisc del dev eth1 root

tc qdisc add dev eth3 root handle 1:0 htb default
tc class add dev eth3 parent 1:0 classid 1:1 htb rate 1Kbit burst 4.2kb cburst 4.2kb
