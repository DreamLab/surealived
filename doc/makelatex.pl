#!/usr/bin/perl -w

use strict;
use Data::Dumper;

my $langhr = ();
my $currlang = '';

if (!defined $ARGV[0]) {
    printf "Usage: $0 infile.doc\n";
    exit 0;
}
my $lst = join ' ', @ARGV;
open IN, "cat $lst |" or die "Can't open $lst";
while(<IN>) {
    if (/^\[(\S+)\]/) {
	next if $1 eq 'ALL';
	$langhr->{$1} = 1;
    }
}
close IN;

foreach my $i (keys %$langhr) {
    my $fh;
    open $fh, ">surealived_$i.tex" or die "Can't open to write surealived_$i.tex";
    $langhr->{$i} = $fh;
    printf "opened: surealived_$i.tex\n";
    printf $fh "\\input preamble.tex\n\n";
    printf $fh "\\begin{document}\n";
}

open IN, "cat $lst |" or die "Can't open $lst";
while(<IN>) {
    if (/^\[(\S+)\]/) {
	$currlang = $1;
	next;
    }

    if ($currlang eq 'ALL') {
	foreach my $i (keys %$langhr) {
	    printf {$langhr->{$i}} "%s", $_;
	}
    }
    else {
	printf {$langhr->{$currlang}} "%s", $_;
    }
}

foreach my $i (keys %$langhr) {
    printf {$langhr->{$i}} "\n\\end{document}";
}
