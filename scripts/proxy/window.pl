#!/usr/bin/perl

use strict;
use warnings;

use constant {
	WINDOW_SIZE => 1024,
};


my $log = "/home/adizere/Projects/measurements/reuse/rtt-eth0.log";
open my $fh, "<", "$log" || die "Cannot open log at $log\n";


my @window;
my $index = 0;
my $loaded = 0;

my $sum;
my $average;

while(<$fh>){
	chomp $_;

	if ($loaded) {
		$sum -= $window[$index];
	}

	$window[$index] = $_;
	$sum += $_;

	# set the loaded flag if filled
	$loaded = 1 if (($loaded == 0) && ($index+1 == WINDOW_SIZE));

	if ( $loaded ){
		my $average = $sum / WINDOW_SIZE;
		print "$average\n";
	}

	# reset index at 0 if reached the end
	$index = ($index+1 == WINDOW_SIZE ? 0 : $index+1);
}
