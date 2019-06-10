#!/usr/bin/perl -w

# Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
# Copyright (c) 2019, Fortian Inc.
# All rights reserved.

# See the file LICENSE which should have accompanied this software.

use strict;
use Data::Dumper;
use POSIX;
use Time::HiRes;
use Compress::Zlib;

use constant INTERVAL => 10;
use constant DEBUG => 0;
use constant VERSION => '1.6';
use constant MARGIN => 26;
use constant HEIGHT => 530;

my %flow = (
    1     => 'C',
    2     => 'M',
    9999  => '1',
    7636  => '2',
    15001 => '3',
    1200  => '4',
    15023 => '5',
    15029 => '8',
    1012  => '7',
);

sub roundup($) {
    my $x = shift;
    my $rv = 1;

    foreach (1 .. 12) {
        $rv *= 10;
        foreach (1 .. 9) {
            if ($x < ($_ * $rv)) {
                return $_ * $rv;
            }
        }
    }
    return 9 * $rv; # well, return it anyway
}

sub pretty($) {
    my $x = shift;

    if ($x < 1000) {
        return sprintf("%4g", $x);
    } elsif ($x < 1000000) {
        return sprintf("%3gK", $x / 1000);
    } elsif ($x < 1000000000) {
        return sprintf("%3gM", $x / 1000000);
    } else {
        return sprintf("%3gG", $x / 1000000000);
    }
}

my %data = ();
my $sip;
my $tndir;

my %aggs = ();

select STDERR;
$| = 1;
select STDOUT;

if ($#ARGV == -1) {
    die "You must supply the prefix to the TN-XX directories.\n";
} elsif (not -d "$ARGV[0]") {
    die "The supplied directory ($ARGV[0]) does not exist.\n";
} elsif (not -d "$ARGV[0]/TN-01") {
    die "The supplied directory ($ARGV[0]) does not hold TN-XX directories.\n";
} else {
    $tndir = shift;
}

print "tndir: $tndir\n" if DEBUG;

my @logs;
opendir DIR, "$tndir" or die "Could not open $tndir: $!\n";
@logs = sort grep { /^TN-/ and (-f "$tndir/$_/runtime.log" or -f "$tndir/$_/runtime.log.gz") } readdir DIR;
closedir DIR;

if (not scalar @logs) {
    die "Couldn't find any log files - giving up.\n";
}

my $c;
my $delta = 0;
my $lastn;
foreach (@logs) {
    my $fname = $_;
    my $id;
    my $comp;
    my $fh;
    if ($fname =~ /TN-(\d+)/) {
        $id = $1;
    } else {
        warn "Could not extract ID from $fname, skipping.\n";
        next;
    }
    if (DEBUG) {
        my @ticks = POSIX::times();
        print "$fname: " . join(', ', @ticks);
        my $sum = 0;
        foreach (@ticks) { $sum += $_; }
        print ': ' . ($sum - $delta) . "\n";
        $delta = $sum;
    } else {
        print STDERR sprintf("\rNow loading %s (%3.2f%% complete)...  ",
            $fname, $delta / scalar @logs);
        $delta += 100;
    }
    $comp = -f "$tndir/$fname/runtime.log.gz";
    if ($comp) {
        unless ($fh = gzopen("$tndir/$fname/runtime.log.gz", "rb")) {
            warn "Could not open $tndir/$fname.gz: $!\n";
            next;
        }
    } else {
        unless (open($fh, "<$tndir/$fname/runtime.log")) {
            warn "Could not open $tndir/$fname: $!\n";
            next;
        }
    }
    my $thr = 0;
    my @scr = (qw(/ - \\ |));
    my $line;
    while (($comp ? ($fh->gzreadline($line) > 0) : ($line = <$fh>))) {
        $thr++;
        $thr %= 5000 * scalar @scr;
        print STDERR "\010" . $scr[$thr / 5000] if (not $thr % 5000);
        my ($when, $holdsip, $dport, $len, $proto);
        ($when, $holdsip, undef, undef, $dport, $len, $proto) = split /:/,
            $line;
        if ($proto != 61) {
            $len *= 8;
        } elsif ($dport == 2) {
            $len *= 1024 * 256;
        }
	next if $proto == 61; # Don't send statistical reports right now

        if (defined $flow{$dport}) {
            $c = $flow{$dport};
        } else {
            $c = 'X';
        }
        if (defined $data{$when}) {
            $data{$when} .= "\n";
        } else {
            $data{$when} = '';
        }
        $data{$when} .= sprintf('%d.%06d:', Time::HiRes::gettimeofday);
        $id =~ s/^0//;
        if ($c =~ /^[0-9]*$/) {
            $data{$when} .= "$id:6:$len:0\n";
            $data{$when} .= "$id:$c:" . int($len * 0.95) . ":1\n";
        }
        $data{$when} .= "$id:$c:$len:0";
    }
    if ($comp) {
        $fh->gzclose();
    } else {
        close $fh;
    }
    $lastn = $id;
}
print STDERR "\rAll logfiles loaded, now sorting and beginning replay...\n";

$| = 1;

my @foo;
my $st = 0;
foreach my $when (sort keys %data) {
    if ($st) {
        @foo = split /:/, $data{$when};
        print STDERR "\rSleeping for " . ($when - $st) . " seconds to send " .
            "data for $foo[0] at $when..." if $when - $st > 1;
        Time::HiRes::sleep($when - $st);
    }
    $st = $when;
    print "$data{$when}\n";
}

print STDERR "\nDone!\n";
