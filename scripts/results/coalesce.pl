#!/usr/bin/perl

use strict;
use warnings;


use Data::Dumper;


die "tc_total file not provided" unless ($ARGV[0] && -r $ARGV[0]);
die "bandwidth file 1 not provided" unless ($ARGV[1] && -r $ARGV[1]);
die "bandwidth file 2 not provided" unless ($ARGV[2] && -r $ARGV[2]);


open my $tc, "<", "$ARGV[0]" || die "Can't open $ARGV[0]\n";
open my $bw0, "<", "$ARGV[1]" || die "Can't open $ARGV[1]\n";
open my $bw1, "<", "$ARGV[2]" || die "Can't open $ARGV[2]\n";


while (<$tc>) {

	chomp $_;
	my @flows_col = split / /, $_;

	my $id = $flows_col[0];
	my $count0 = $flows_col[1] || 1;
	my $count1 = $flows_col[2] || 1;

	my $instant_bw0 = $1 if (<$bw0> =~ /TCP_BW: (\d+)/);
	my $instant_bw1 = $1 if (<$bw1> =~ /TCP_BW: (\d+)/);

	die "Bandwidth feed finished" unless ( $instant_bw0 || $instant_bw1 );

	printf("%d %.3f %.3f\n", $id,
		$instant_bw0/$count0, $instant_bw1/$count1);
}
