#/bin/bash


init_shaper() {
    # reset previous settings
    $(tc qdisc del dev eth0 root 2>/dev/null)
    $(tc qdisc del dev eth1 root 2>/dev/null)
    $(tc qdisc del dev eth2 root 2>/dev/null)
    $(tc qdisc del dev eth3 root 2>/dev/null)

    # delay with a random variation of max 20 ms and 25% correlation
    tc qdisc add dev eth0 root netem delay 50ms 10ms 25%
    tc qdisc add dev eth2 root netem delay 50ms 10ms 25%

    # echo " * Initialized to: ";
    tc qdisc show dev eth0
    tc qdisc show dev eth2
}

modify_delay() {
    dev=$1
    delay="$2ms"
    variation="$(expr $2 / 2)ms";

    tc qdisc change dev $dev root netem delay $delay $variation loss 10%
    echo "Delay modified for $dev: ";
    tc qdisc show dev $dev
}


modify_bandwidth() {
    dev=$1
    bw=$2
    latency=$3

    tc qdisc replace dev $dev root tbf rate $bw burst 16KB latency $latency

    echo "Bandwidth modified for $dev: ";
    tc -s -d qdisc show dev $dev
}

init_shaper;



while true ; do
    modify_bandwidth 'eth3' 3500Kbit 120ms
    modify_bandwidth 'eth1' 2450Kbit 160ms

    sleep 30;

    modify_bandwidth 'eth3' 2450Kbit 160ms
    modify_bandwidth 'eth1' 3500Kbit 120ms

    sleep 30;
done
