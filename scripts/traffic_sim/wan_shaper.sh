#/bin/bash



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

    echo "$dev: $bw";
    #tc -s -d qdisc show dev $dev
}

init_shaper() {
    # reset previous settings
    $(tc qdisc del dev eth0 root 2>/dev/null)
    $(tc qdisc del dev eth1 root 2>/dev/null)
    $(tc qdisc del dev eth2 root 2>/dev/null)
    $(tc qdisc del dev eth3 root 2>/dev/null)

    # delay with a random variation of max 20 ms and 25% correlation
    tc qdisc add dev eth0 root netem delay 40ms
    tc qdisc add dev eth2 root netem delay 40ms

    modify_bandwidth 'eth1' 3000Kbit 120ms
    modify_bandwidth 'eth3' 3000Kbit 120ms


    # echo " * Initialized to: ";
    tc qdisc show dev eth0
    tc qdisc show dev eth2
}

init_shaper;


# while true ; do
#     echo "--- " $(date);
#     modify_bandwidth 'eth1' 1500Kbit 160ms
#     echo "---";
#     sleep 30;

#     echo "--- " $(date);
#     modify_bandwidth 'eth1' 3000Kbit 120ms
#     echo "---";
#     sleep 30;


#     echo "--- " $(date);
#     modify_bandwidth 'eth3' 1500Kbit 160ms
#     echo "---";
#     sleep 30;

#     echo "--- " $(date);
#     modify_bandwidth 'eth3' 3000Kbit 120ms
#     echo "---";
#     sleep 30;

# done

sleep 50;
modify_bandwidth 'eth3' 1500Kbit 160ms
