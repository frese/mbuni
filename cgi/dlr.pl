#!/usr/bin/perl -w
#----------------------------------------------------------------------
# CGI script for registering DLR's
#
#----------------------------------------------------------------------

use CGI qw(:standard);

my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst, $date);
my ($to, $from, $smsc, $text, $dlr, $dlr_id);

# Get time/date
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
$date = sprintf("%04d-%02d-%02d %02d:%02d:%02d", $year+1900,$mon+1,$mday,$hour,$min,$sec);

# Get params
$msisdn = param('msisdn');
$smsc   = param('smsc');
$text   = param('text');
$dlr    = param('dlr');
$dlr_id = param('dlr_id');

open LOG,">>/tmp/dlr.log";
print LOG "$date DLR [msisdn:$msisdn] [smsc:$smsc] [dlr:$dlr] [id:$dlr_id] [text:$text]\n";
close LOG;

print header('text/plain');
print "OK\n";
