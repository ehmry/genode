use strict;

my $out = $ENV{"out"};

my %loc = ();
my %sys = ();

foreach (glob $ENV{"file"}) {
    open SRC, "<$_" or die "$!, $_";
    while (<SRC>) {
        if (/^\#include\s+\"([^\"]*)\"/) {
            $loc{"\"$1\"\n"} = 1;
        }
        if (/^\#include\s+\<([^\>]*)\>/) {
            $sys{"\"$1\"\n"} = 1;
        }
    }
    close SRC;
}

my @loc = keys %loc;
my @sys = keys %sys;

open OUT, ">$out" or die "$!";
print OUT "{local=[\n @loc];\nsystem=[\n @sys];}\n";
close OUT;
