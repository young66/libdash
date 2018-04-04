#!/usr/bin/perl

use 5.018;
use warnings;
use strict;

my $outifc = "lo";

# get the target [bandwidth|delay|packet_loss_rate] from command line
# bandwidth in bit
# delay in ms
# packet loss rate in [0-1]
my ($tbw, $tdl, $tpl) = (@ARGV);

$tbw = 0 unless defined $tbw;
$tdl = 0 unless defined $tdl;
$tpl = 0 unless defined $tpl;


$tpl *= 100.0 if $tpl != 0;

# clean up all stuff
# `sudo tc qdisc del dev $inifc ingress 2>&1`;
# `sudo tc qdisc del dev ifb0 root 2>&1`;
`sudo tc qdisc del dev $outifc root 2>&1`;

if ( $tbw  != 0 ){

    my $tdl_var = $tdl * 0.1;
    say "\n" . '=' x 50;
    say "throttle bandwidth to $tbw bit";
    say "impose transmission delay to $tdl ms +/- $tdl_var ms ";
    say "dump packets in rate $tpl %";
    say '=' x 50;

    # setup the root rule and divide the bandwidth
    `sudo tc qdisc add dev $outifc root handle 1: htb default 11`;
    `sudo tc class add dev $outifc parent 1: classid 1:1 htb rate 10mbps ceil 10mbps`;
    `sudo tc class add dev $outifc parent 1:1 classid 1:10 htb rate $tbw ceil $tbw burst 50 cburst 50`;
    # `sudo tc class add dev $outifc parent 1:1 classid 1:10 htb rate $tbw ceil $tbw`;
    `sudo tc class add dev $outifc parent 1:1 classid 1:11 htb rate 5mbps ceil 5mbps`;
    `sudo tc class add dev $outifc parent 1:1 classid 1:12 htb rate 5mbps ceil 5mbps`;

    # filter the http packets
    `sudo tc filter add dev $outifc parent 1:0 protocol ip prio 1 u32 match ip sport 8444 0xffff flowid 1:10`;
    `sudo tc filter add dev $outifc parent 1:0 protocol ip prio 1 u32 match ip dport 8444 0xffff flowid 1:12`;
#    `sudo tc filter add dev $outifc parent 1:0 protocol ip prio 1 u32 match ip sport 8444 0xffff match ip dport 8444 0xffff flowid 1:10`;

    # impose traffic delay
    `sudo tc qdisc add dev $outifc parent 1:10 handle 10: netem delay ${tdl}ms loss ${tpl}%`;
    `sudo tc qdisc add dev $outifc parent 1:12 handle 12: netem delay ${tdl}ms loss ${tpl}%`;
    # `sudo tc qdisc add dev $outifc parent 1:12 handle 12: netem delay ${tdl}ms ${tdl_var}ms distribution normal loss ${tpl}%`;
    # `sudo tc qdisc add dev $outifc parent 1:12 handle 12: netem delay ${tdl}ms ${tdl_var}ms distribution normal loss ${tpl}%`;

    # default traffic
    `sudo tc qdisc add dev $outifc parent 1:11 handle 11: sfq perturb 10`;

    # setup rules on ifb0
    # my $bucket = $tbw / 8;
    # `sudo tc qdisc add dev ifb0 root handle 1: tbf rate $tbw burst $bucket latency 120s`;
    # `sudo tc qdisc add dev ifb0 handle 1: root htb default 11`;
    # `sudo tc class add dev ifb0 parent 1: classid 1:1 htb rate $tbw`;
    # `sudo tc class add dev ifb0 parent 1:1 classid 1:11 htb rate $tbw`;
    # `sudo tc qdisc add dev ifb0 parent 1:11 handle 11: pfifo`;
    # `sudo tc qdisc add dev ifb0 parent 11: handle 12: netem delay ${tdl}ms loss ${tpl}%`;

    # redirect the receiving packets to the ifb0 device
    # `sudo tc qdisc add dev $inifc ingress`;
    # `sudo tc filter add dev $inifc parent ffff: protocol ip u32 match u32 0 0 flowid 1:1 action mirred egress redirect dev ifb0`;
}

say "\n" . '=' x 100;
print `sudo tc qdisc show`;
say '=' x 100;
