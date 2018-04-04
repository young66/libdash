#!/usr/bin/perl

use strict;
use 5.018;
use warnings;

# 51375, 6.27 KB/s
# 195367, 23.85 KB/s
# 515326, 62.91 KB/s
# 771196, 94.14 KB/s

# 10 KB/s, 81920 bps
# 200 KB/s, 1638400 bps
# 40 KB/s, 327680
#my $timeIntv = 15;
my $timeIntv = 30;

# my @bws = qw/60kbps 80kbps 100kbps/;
# my @lts = qw/10 150 250/;
#my @bws = qw/100kbps 80kbps 60kbps 40kbps 100kbps 80kbps 60kbps/;
my @bws = qw/60kbps 200kbps 60kbps 200kbps/;
my @lts = qw/10/;

my ($lastbw, $lastlt) = ("0kbps", 0);
`./adjust.pl`;

my ($bw, $lt) = ("200kbps", 10);
say "switch to $bw, ", $lt * 2, " ms";
`./adjust.pl $bw $lt 2>&1`;

for my $lt (@lts){
    for my $bw (@bws){
        sleep $timeIntv;
        say "switch to $bw, ", $lt * 2, " ms";
        `./adjust.pl $bw $lt 2>&1`;
    }
}
# while(1){
#     sleep $timeIntv;
#     my ($bw, $lt);
#     do{
#         $bw = $bws[int rand @bws];
#         $lt = $lts[int rand @lts];
#     }while($bw eq $lastbw || $lt == $lastlt);
#     $lastbw = $bw;
#     $lastlt = $lt;
#     say "switch to $bw, ", $lt * 2, " ms";
#     `./adjust.pl $bw $lt 2>&1`;
# }
