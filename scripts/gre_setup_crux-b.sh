#!/bin/bash

# GRE tunnel pre-requisistes & setup for site B

set -e


modprobe gre
modprobe ip_gre

ip tunnel add tun0 mode gre remote 192.168.0.2 local 192.168.1.2 ttl 255
ip link set tun0 up
ip addr add 10.0.1.2 dev tun0
ip route add 10.0.1.0/24 dev tun0
