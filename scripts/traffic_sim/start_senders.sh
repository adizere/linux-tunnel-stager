#!/bin/bash

RECEIVER="10.0.1.2"

count=32;
port=5002;


while [[ $count > 0 ]]; do
	./sender $RECEIVER $port &
	count=$(expr $count - 1);
	port=$(expr $port + 1);
	echo "Still have $count instances to start";
	sleep 0.2;
done
