#!/usr/bin/perl -w

use English;
use Data::Dumper;
use Socket;
use Sys::Hostname;
use POSIX;
use Net::HTTP;
use strict;

my $debug = 0;

$ENV{'PATH'} = '/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin';

#################################################################################
if ( scalar @ARGV < 4 ) {
    print "Uzycie: $0 HOST PORT HOST_HEADER PATH [--verbose]\n";
    print " ex:\n";
    print "\t $0 www.onet.pl 80 www.onet.pl /0\n";
    print "Zwraca:\n";
    print " 0 - tester OK\n";
    print " 1 - tester ERROR\n";
    exit 0;
}

if (scalar @ARGV == 5 && $ARGV[4] eq '--verbose') {
    $debug = 1;
}

my $host = $ARGV[0];
my $port = $ARGV[1];
my $hosthdr = $ARGV[2];
my $path = $ARGV[3];

use IO::Socket;
my $sock = IO::Socket::INET->new(PeerAddr => $host, PeerPort => $port,
				 Timeout => 10, Proto => 'tcp');

exit 1 unless ($sock); # 

$sock->print("GET $path HTTP/1.0\r\nHost: $hosthdr\r\n\r\n");
my $resp = <$sock>;
$resp =~ /^\S+\s+(\d+)/;
if ($debug) {
    print $resp;
    print while(<$sock>);
    print "\n";
}

exit 0 if ($1 == 200);
exit 1;

