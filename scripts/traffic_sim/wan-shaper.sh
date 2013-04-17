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
    delay=$2

    tc qdisc change dev $dev root netem delay $delay 20ms 25% loss 7%
}


init_shaper;

while true ; do
    sleep 2;

    modify_delay 'eth3' 250
    echo "Delay for eth3 increased";

    sleep 2;

    modify_delay 'eth3' 50
    echo "Delay for eth3 decreased";

    sleep 30;
done



