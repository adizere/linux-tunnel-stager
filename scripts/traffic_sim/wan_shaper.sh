#/bin/bash


init_shaper() {
    # reset previous settings
    $(tc qdisc del dev eth1 root 2>/dev/null)
    $(tc qdisc del dev eth3 root 2>/dev/null)

    # delay with a random variation of max 20 ms and 25% correlation
    tc qdisc add dev eth1 root netem delay 100ms 20ms 25% loss 7%
    tc qdisc add dev eth3 root netem delay 100ms 20ms 25% loss 7%

    echo " * Initialized to: ";
    tc qdisc show dev eth1
    tc qdisc show dev eth3
}

modify_delay() {
    dev=$1
    delay="$2ms"
    variation="$(expr $2 / 2)ms";

    tc qdisc change dev $dev root netem delay $delay $variation loss 7%
    echo "Delay modified for $dev: ";
    tc qdisc show dev $dev
}


init_shaper;
sleep 5;
while true ; do
    modify_delay 'eth3' 250
    modify_delay 'eth1' 100

    sleep 20;

    modify_delay 'eth3' 100
    modify_delay 'eth1' 250

    sleep 20;
done
