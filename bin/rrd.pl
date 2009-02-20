#!/usr/local/bin/perl
#---------------------------------------------------------------
# Script to update or generate a new RRD database for every
# Realgate instance running on this server, as defined by
# /etc/realgate.def.
#
# The RRD databases are used to collect statistically data from 
# the access logs. How many messages are sent/received per sec.
#
# This script should be called from cron every minute. It will
# then create a RRD database, if it doesn't exists, and update
# it with the data from the last minute.
#
# The precission of the data:
# The last 48 hour, every minute.
# The last month, every hour.
# The last year, every day.
#
#
# (c) Jan 2004, Allan Frese, Realtime A/S
#---------------------------------------------------------------

# Get server specific information
my $rrdtool="/usr/local/bin/rrdtool";
$def_file = "/etc/mbuni.def";
read_def();

# Get the last minut
my $lastmin;
my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time-60);
$lastmin = sprintf("%04d-%02d-%02d %02d:%02d", $year+1900,$mon+1,$mday,$hour,$min);

# Loop through every defined instance

foreach $instance (split(/\,/,$def{INSTANCES})) {

    # test if the RRD database exists, if not we will create it.
    if (! -f "$def{MBUNI_HOME}/rrd/$instance.rrd") {
        $cmd = "$rrdtool create $def{MBUNI_HOME}/rrd/$instance.rrd ".
	       "--step 60 ".                 # we messure every 60 sec.
	       "DS:sent:GAUGE:120:0:U ".     # the values for sent and receive
	       "DS:recv:GAUGE:120:0:U ".
	       "DS:queue:GAUGE:120:0:U ".
	       "RRA:AVERAGE:0.5:1:2880 ".    # average every minute (2880 sec = 48 hours)
	       "RRA:AVERAGE:0.5:60:744 ".    # average every hour for a month (24*31 = 744)
	       "RRA:AVERAGE:0.5:1440:365";   # average every day for a year (1440 sec = 24 hours)

	system($cmd);

	# test if we did succeed ...
	if ( ! -f "$def{MBUNI_HOME}/rrd/$instance.rrd" ) {
	    print "RRD Database create failure !!!\n";
	    exit;
	}
    }

    # Now update the RRD database.
    $sent=`tail -5000 $def{MBUNI_HOME}/log/access.$instance.log | grep '^$lastmin' | grep 'Sent MMS' | wc -l|tr -d ' '`;
    $recv=`tail -5000 $def{MBUNI_HOME}/log/access.$instance.log | grep '^$lastmin' | grep 'Received MMS' | wc -l|tr -d ' '`;
    chomp($sent);
    chomp($recv);

    # check if new format - with queue size !
    $cmd = "$rrdtool info $def{MBUNI_HOME}/rrd/$instance.rrd";
    $n = `$cmd | grep -c "ds\[queue\]"`;
    if ($n eq 0) {
	$cmd = "$rrdtool update $def{MBUNI_HOME}/rrd/$instance.rrd ".
	       "-t sent:recv N:$sent:$recv > /dev/null";
	system($cmd);

    } else {
	$que = get_queue_size();
	$cmd = "$rrdtool update $def{MBUNI_HOME}/rrd/$instance.rrd ".
	       "-t sent:recv:queue N:$sent:$recv:$que > /dev/null";
	system($cmd);
    }

}

#--------------------------------------------------------------------
sub get_queue_size {

    $CFG = "$def{MBUNI_HOME}/etc/mbuni.$instance.conf";
    $BROWSER='/usr/local/bin/lynx -connect_timeout=10 -mime_header';

    my $port=`grep "^admin-port *= *" $CFG`;
    $port=~ /\=\s*([0-9]*)/;
    $port = $1;
    @out = `$BROWSER 'http://localhost:$port/status.txt'`;
    $que = 0;
    foreach $line (@out) {
	if ($line =~ /received \d+ \((\d+) queued\)/) {
	    $que += $1;
	}
	if ($line =~ /sent \d+ \((\d+) queued\)/) {
	    $que += $1;
	}
    }
    return $que;
}


#--------------------------------------------------------------------
# Read the "/etc/realgate.def" file to determin where kannel
# is installed, which user, password etc.
sub read_def
{
    open(FH, "<$def_file") || ERROR("Can't open $def_file");
    while ($line = <FH>) {
        chop($line);
        next if ($line =~ /^#/);  # skip comments
        if ($line =~ /=/) {
            ($var,$val) = split(/=/,$line);
            $val = squeeze($val);
            $var = squeeze($var);

            $def{$var} = $val;
        }
    }
    close(FH);
}

#------------------------------------------------------------------
# Sub         : squeeze
# Arguments   : string, remove
# Returns     : string
# Description : removes whitespace, with respect for quotes (" & ')
#               and escaped characters, including (\" & \')
#------------------------------------------------------------------
sub squeeze
{
    my ($line)   = shift;
    #my ($remove) = shift;
    my ($out)    = "";
    my ($first, $match);

    while ( $line )
    {
        if ( $line =~ m/("|')((\\.|[^\1])*)\1/ )
        {
            $line  = $';
            $match = $2; #$&;
            #$match = ($remove?$2:$&);
            ($first = $`) =~ s/\s+//g;

            $out .= $first . $match ;
        }
        else
        {
            ($out .= $line) =~ s/\s+//g;
            $line = "" ;
        }
    }

    return $out;
}


