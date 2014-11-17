#!/usr/bin/env perl
use strict;
use warnings;

my $usage = "usage: $0 <[name].glsl>\n";

$ARGV[0] or die($usage);

my $filename = $ARGV[0];
$filename =~ /^([^.]+)\.glsl$/ or die($usage);
my $name = $1;

-e $filename or die("no such file: $filename\n");

my $out = "";

for my $type ("vert", "frag") {
	$out .= "static const char* ${name}_${type}_src =\n";

	my $current_type = "";
	my $found = 0;

	open IN, $filename;
	while (<IN>) {
		chop;
		if (/^@(.*)$/) {
			$current_type = $1;
		} elsif ($current_type eq $type) {
			$out .= "\t\"$_\\n\"\n";
			$found = 1;
		}
	}
	$out .= ";\n";
	close IN;

	die("$filename: found no \@$type section\n") if !$found;
}


open OUT, ">$filename.inc";
print OUT $out;
close OUT;
