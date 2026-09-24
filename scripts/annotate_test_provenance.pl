#!/usr/bin/env perl
# Annotate each TEST()/TEST_F() lacking per-test provenance by propagating the
# file-level // Ported from: / // Authored: header. Implements CLAUDE.md §5.
#
# Usage: perl annotate_test_provenance.pl <test.cpp> [<test.cpp> ...]
use strict;
use warnings;

my @files = @ARGV;
die "usage: $0 <test.cpp> [...]\n" unless @files;

for my $file (@files) {
    open my $fh, '<', $file or do { warn "skip $file: $!"; next; };
    my @lines = <$fh>;
    close $fh;

    # 1. Capture file-level provenance line (first // Ported from|Authored in header).
    my $prov;
    for my $i (0 .. 12) {
        last if $i > $#lines;
        if ($lines[$i] =~ /^(\/\/\s*(?:Ported from|Authored).*[^\n]*\n?)/) {
            $prov = $1;
            $prov =~ s/\s*\n?$//;   # trim trailing newline
            last;
        }
    }
    unless (defined $prov) {
        warn "skip $file: no file-level provenance header\n";
        next;
    }
    my $is_ported = ($prov =~ /Ported from/);

    # 2. For each TEST/TEST_F without provenance in the preceding 6 lines, insert.
    my @out;
    my $inserted = 0;
    for (my $i = 0; $i <= $#lines; $i++) {
        if ($lines[$i] =~ /^\s*TEST(_F|_P)?\s*\(\s*(\w+)\s*,\s*(\w+)/) {
            my ($suite, $case) = ($2, $3);
            my $has = 0;
            for (my $j = ($i >= 6 ? $i - 6 : 0); $j < $i; $j++) {
                if ($lines[$j] =~ /Ported from|Authored/) { $has = 1; last; }
            }
            if (!$has) {
                push @out, "$prov\n";
                push @out, "//              TEST($suite, $case)\n" if $is_ported;
                $inserted++;
            }
        }
        push @out, $lines[$i];
    }

    if ($inserted > 0) {
        open my $ofh, '>', $file or do { warn "write $file: $!"; next; };
        print $ofh @out;
        close $ofh;
        print "$file: +$inserted annotations\n";
    }
}
