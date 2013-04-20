#!/bin/bash

count=32;
port=5002;

while [[ $count > 0 ]]; do
	./receiver $port &
	count=$(expr $count - 1);
	port=$(expr $port + 1);
	echo "Still have $count instances to start";
done
