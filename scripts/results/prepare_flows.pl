#!/usr/bin/perl


use strict;
use warnings;


use Data::Dumper;


die "tc_total file not provided" unless $ARGV[0];

my $tc_total = $ARGV[0];
die "Can't read $tc_total\n" unless -r $tc_total;


open my $fh, "<", "$tc_total" || die "Can't open $tc_total\n";


my $c = 0;
while (<$fh>) {
	my @column = ($_ =~ /\[ ?(\d+\.\d+)\].* tc (\d+) (\d+)/);

	print $column[0] . " " . $column[1] . " " . $column[2] . "\n";


}
