#!/bin/bash

tc qdisc add dev eth0 root tbf rate 1200Kbit burst 10000 latency 120ms
tc -s -d qdisc show dev eth0

