#!/bin/bash

# Measures how much bandwidth is utilized in each second for a given net if

dev="eth0";
steps=1;

if [[ $# > 0 ]]; then
	dev="$1"
fi

echo " * Watching the bandwidth usage for interface: $dev";

# RX and TX bytes at start time
rx_last=$(ifconfig $dev | grep bytes | awk '{print $2}' | sed 's/bytes://g' );
tx_last=$(ifconfig $dev | grep bytes | awk '{print $6}' | sed 's/bytes://g' );

echo -e '#\tTime\t\tRX bytes\tTX bytes';
sleep 1;

while [[ true ]]; do

	# Values for RX and TX are recorded in each second
	rx_instant=$(ifconfig $dev | grep bytes | awk '{print $2}' | sed 's/bytes://g' );
	tx_instant=$(ifconfig $dev | grep bytes | awk '{print $6}' | sed 's/bytes://g' );

	# Compute how much bandwidth was used in the last second
	rx=$(expr $rx_instant - $rx_last);
	tx=$(expr $tx_instant - $tx_last);

	# preserve the last values, we'll need them at the next iteration
	rx_last=$rx_instant;
	tx_last=$tx_instant;

	echo -e "$steps\t$(date +%T)\t$rx\t\t$tx";

	steps=$(expr $steps + 1);
	sleep 1;
done