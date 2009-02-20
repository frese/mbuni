#!/usr/bin/env /usr/local/bin/perl
#--------------------------------------------------------------------------
# mbuni, Atchik-Realtime MMS Gateway, Administration module
#
# Produces several web-pages to administer a mbuni configuration file
# Should be run as cgi script under a webserver like apache
#
# The web-server part of the installation (Apache) :
#
# You will need <Directory> and "Alias" specifications for the 
# cgi script directory.
#
#    Alias /mbuni/ "/usr/local/mbuni/cgi/"
#
#    <Directory "/usr/local/mbuni/cgi">
#        DirectoryIndex index.pl
#        Options ExecCGI
#        AddHandler cgi-script .pl
#        AllowOverride None
#        Order deny,allow
#        Deny from All
#        Allow from <your place>
#    </Directory>
#
#
# (c) Realtime A/S, Allan Frese
#--------------------------------------------------------------------------

package Kannel;
#use strict;

# Modules
use Sys::Hostname;
use CGI;
use HTML::Template;
use LWP::UserAgent;
use Data::Dumper;
use File::Basename;
use English;

# some global vars
use vars qw($line $val $options $cgi_var $cgi_ctrl $found $v $encrypt @list 
            $frmctrl $help $group_ $var @parameters $tmp @instances
	    $config_file $def_file %def $q $timeout %config %cgi_params $rm $lrm
	    $digest $instance $pname);

$| = 1;  # skip the output buffer



# Initialize "forms", this is done only once under mod_perl
# it's heavy and it doesn't change !!


# "forms" to edit Kannel configuration file
#
# This is the key-structure for handling the configuration file, how it should be shown
# on the display, which parameters is mandatory etc.
#
#
# varname        : The parameter name as defined i kannel configuration file
#
# form-ctrl      : This controls when the element should be shown, or it controls 
#                  the visibillty of other elements
#                  ctrl     - controls visibillity of other elements
#                  hctrl    - controls visibillity of other elements 
#                             (this element hidden in the configuration file)
#                  xxxx     - value of control when it should be shown
#                  "blank"  - always shown, doesn't control other elements
#
# type           : The field type
#                  readonly - readonly
#                  number   - the input field can only contain numbers
#                  string   - the input field can contain any thing
#                  ip       - an IP address
#                  select   - a drop-down box
#                  mselect  - a multi-select box
#                  bool     - a true/false value
#
# default        : The default value unless otherwise specified
#
# size           : The field size (typically the length) of the screen element
#
# mandatory      : Flag indicating whether or not this is a mandatory parameter
#
# options        : A list of options for "select" and "mselect" where '&' indicates a function reference
#                  that returns an option list
#
# help message   : Optional help message
#

my $forms = { 
    'core' =>
	# [ varname                     ,form-ctrl , [ type, default, size, mandatory, options.. ], "help message"]
        [ ['group'		        ,''        , ['readonly','core',0,1]],
	  ['log-file'		        ,''        , ['string','bearerbox.log',60], "The core debug log-file."],
	  ['log-level'		        ,''        , ['number','1',5], "Number 0..5. Minimum level of logfile events logged. 0 is for 'debug', 1 'info', 2 'warning, 3 'error' and 4 'panic'"],
	  ['log-rotate'		        ,''        , ['select','weekly',0,0,'','daily','weekly','monthly'], "Enable log-rotation for the core (bearerbox) debug and access logs. You can do this every day, week or month."],
	  ['access-log-rotate'		        ,''        , ['select','weekly',0,0,'','daily','weekly','monthly'], "Enable log-rotation for the core (bearerbox) debug and access logs. You can do this every day, week or month."],
	  ['access-log'		        ,''        , ['string','access.log',60], "Choose  filename for the bearerbox access log. This log is used for all statistically information."],
#	  ['correlation-url'            ,''        , ['string','',40], "Url where to get an id range. (defaults to http://app.realtime.dk/...)"],
	], 
    'mbuni' =>
        [ ['group'      		,''        , ['readonly','mbuni',0,1]],
	  ['name'		        ,''        , ['string','',30,0], "User-friendly name for the Gateway, used in notices, etc"],
	  ['storage-directory'		,''        , ['string','',30,0], "Directory where Mbuni creates message queues, MMBoxes, and the User Agent profiles cache. Mbuni creates a set of sub-directories in here for each function"],
	  ['max-send-threads'		,''        , ['number','',5],    "How many queue processing threads to start. A higher value means messages get delivered faster."],
	  ['maximum-send-attempts'	,''        , ['number','',5],    "Maximum number of attempts gateway must make to deliver message before giving up (e.g. mobile phone is off, VAS service is down, email domain is unavailable)"],
          ['max-memory-queued'		,''	   , ['number','',5],	"How many requests may be 'live' queued in memory. Since each live request consumes a number of filehandles, this should be significantly lower than the process limit. The default setting is a tenth of the process limit. (Optional.)"], 
	  ['unified-prefix'		,''        , ['string','',30],   "A string to unify received phone numbers, so that routing works correctly. Format is that first comes the unified prefix, then all prefixes which are replaced by the unified prefix, separated with comma (','). For example '+256,000256,0;+,000' should ensure correct UG prefixes. If there are several unified prefixes, separate their rules with semicolon (';')"],
	  ['strip-prefixes'		,''        , ['string','',30],   "snask"],
#	  ['default-message-expiry'     ,''        , ['number','',5],    "Default number of seconds in which message expires and is purged from queue (if not yet delivered). This figure is overridden by whatever is in the message. "],
	  ['queue-run-interval'         ,''        , ['number','',5],    "How many seconds between each queue run."],
	  ['send-attempt-back-off'      ,''        , ['number','',5],    "The exponential back-off factor to employ on subsequent message delivery attempts, when a delivery attempt fails."],
	  ['allow-ip'  	                ,''        , ['string','',60],   "List of IP addresses of hosts allowed to use/access the Send MMS Port (MM1 interface for MMSC or SendMMS port for VAS Gateway). You can use this for instance to insist that only connections coming via a known/trusted WAP gateway are serviced by the MMSC. Leave out to allow all machines to connect."],
	  ['deny-ip'	                ,''        , ['string','',60],   "List of IP addresses of hosts not allowed to use/access the Send MMS Port (MM1 interface for MMSC or SendMMS port for VAS Gateway)."],
	  ['sendmms-port'	        ,''        , ['number','',5],    "The port on which the MMS VAS gateway listens for incoming MMS send requests. (Optional.)"],
	  ['sendmms-port-ssl'           ,''        , ['bool','false'],   "Set to true if SendMMS port of VAS Gateway should be secure (HTTPS). This is only supported if Mbuni is compiled with SSL support."],
	  ['admin-port'				,''			,['number','',5],		"The port on which MBuni listens for administrative commands and serves status reports. "],
	  ['admin-password'		,''			,['string','',60],		"The password required for issuing administrative commands to the admin interface of MBuni"],
	  ['admin-allow-ip'	                ,''        , ['string','',60],    "List of IP addresses of hosts allowed to use/access the admin port"],
	  ['admin-deny-ip'	                ,''        , ['string','',60],    "List of IP addresses of hosts not allowed to use/access the admin port"],
	  ['email-domains'		,'',		,['string','',60], "A comma separated list of email domains handled by this mbuni instance."],
	  ['eaif-http-accept-response', ''	,['number','',10], "Used to control what http response the EAIF module uses, when sending back a response to successful deliver request. Usually (default) this is 204 No content, but sometimes the MMSC will need 200 OK"],
          ],
    'mmsc' =>
        [ ['group'      		,''        , ['readonly','mmsc',0,1]],
	  ['type'			,'ctrl'    , ['select','soap',0,1,'soap','eaif','mpes' ], "The mmsc driver type. Protocol spoken by this MMSC, one of soap (for 3GPP MM7 SOAP) or eaif for Nokia EAIF protocol."],
	  ['id'		 	        ,''        , ['string','',20,1],  "An mandatory name or id for the mmsc. Any string is acceptable, but semicolon ';' may cause problems, so avoid it and any other special non-alphabet characters. Identifying name for this connection (used in logs for instance). For MM7 connections it also used as the VASP ID parameter"],
	  ['group-id'		 	,''        , ['string','',20,0],  "Optional: Can be used to group different MMCs for purposes of receiving DLRs, etc."],

	  ['vasp-id'		 	,''        , ['string','',20,0],  "Optional: Used to specify VASP-ID for a SOAP/MM7 MMSC. When left unspecified the id of the MMSC is used instead."],
	  ['mm7-version'		,''        , ['string','',20,0,], "Optional: The MM7 version to use on this interface. (defaults are '5.3.0' for MM7/SOAP, '3.0' for EAIF.) For SOAP, this is used in the MM7Version tag. For EAIF it is reported in the HTTP headers."],
          ['use-mm7-soap-namespace-prefix', '',	   ,['bool','true'], "Optional: Whether to use the mm7 namespace in soap xml tags. The default behaviour is yes"],
	  ['mm7-soap-xmlns'		,''        , ['string','',50,0,], "Optional: The namespace URI for the 'mm7:' XML namespace. Only valid for MM7/SOAP, and is used in the SOAP message to identify the namespace for all MM7/SOAP specific elements. Default value is http://www.3gpp.org/ftp/Specs/archive/23_series/23.140/schema/REL-5-MM7-1-0"],
	  ['mmsc-url'		        ,''        , ['string','',50,1],  "Mandatory: URL address for the MMSC. Used for outgoing messages"],
          ['incoming-port'              ,''        , ['number','',10,0],  "Port at which Mbuni listens for incoming messages from MMSC."],
          ['incoming-port-ssl'          ,''        , ['bool','false',],   "Specifies whether incoming port is secure (i.e. incoming requests use secure HTTP). Only supported if Mbuni is compiled with SSL support."],
	  ['incoming-username'	        ,''        , ['string','',30],    "Username to be used by MMSC to authenticate itself to Mbuni for incoming connections. (Using HTTP basic authentication scheme.)"],
	  ['incoming-password'	        ,''        , ['string','',30],    "Password to be used by MMSC to authenticate itself to Mbuni for incoming connections"],
	  ['allow-ip'	                ,''        , ['string','',60],    "List of IP addresses of hosts allowed to use/access the incoming MMS port"],
	  ['deny-ip'	                ,''        , ['string','',60],    "List of IP addresses of hosts not allowed to use/access the incoming MMS port"],
          ['allowed-prefix'             ,''        , ['number','',30,0],  "List of recipient number prefixes that can be delivered via this MMSC (used for routing out-going messages)"],
          ['denied-prefix'              ,''        , ['number','',30,0],  "List of recipient number prefixes that cannot be delivered via this MMSC"], ['max-throughput'             ,''        , ['number','',30,0],  "Maximum number of messages per second (outgoing) that this MMSC can handle."], ['mm7-mt-filter-params'       ,''        , ['string','',30],    "Parameter(s) to be passed to the init function of the MT MMS filter module specified in mmsbox-mt-filter-libary  above. (See mmsbox_mt_filter.h for details.) The init function is called once for each MMC connection, and must return no error, otherwise no filtering will be done on MT messages via this MMC."],
	  ['incoming-number-substitutions',''	,['string','',30], "A comma separated list of substitution patterns to apply to incoming numbers on this mmsc"],
	  ['outgoing-number-substitutions',''	,['string','',30], "A comma separated list of substitution patterns to apply to outgoing numbers on this mmsc"],
	  ['match-part-order', '',				,['string','',30], "A comma separated list specifying the manner in which to match messages to services. The allowed tokens are text and subject. Mbuni will try attempt to keyword/regex matching on each of these in the order specified. If a matching service, the search stops. If no service is found after all specified match parts have been searched, then any catch-all service will be used. The default value is text, which only checks in the message text elements."],
	  ['asynchronous-eaif-mode', ''			,['bool','false',], "Specifies whether to send asynchronous responses. Only used when using EAIF protocol."],
	  ['detect-duplicate-messages', ''		,['bool','false',], "If enabled, messages which are duplicates of previous messages will be discarded. It is recommended to use this if using running in asynchronous mode."],
	  ['msgid-expire',	''		,['number', '',30,0], "The number of seconds that a message id is considered unique. If two messages are received within this interval, they are considered duplicates - otherwise not. The default value 24 hours."],
	  ],
    'mms-service' =>
        [ ['group'      		,''        , ['readonly','mms-service',0,1]],
	  ['name'                       ,''        , ['string','',20,1], "Service name. Used for logging, also sent to MMSC as VAS ID parameter for MM7 SOAP requests"],
	  ['match'              ,'hctrl'   , ['select','keyword',0,1,'keyword','regex'], "Choose whether this is a keyword or a regular expression service. Note that regular expressions are evaluated in the same order they appear here (in the configuration file)."],
	  ['keyword'			,'keyword'        , ['string','',40,1], "mandatory keyword. Incoming messages whose text part matching this parameter will be routed via this service."],
	  ['aliases'			,'keyword'        , ['string','',40],   "Optional keyword aliases seperated by semi-colon ';'."],
	  ['regex'                      ,'regex'   , ['string','',40], "A regular expression, using POSIX Extended Regular Expression syntax. The search is case insensitive."],
	  ['servicetype'		,'hctrl'   , ['select','post-url',0,1,'post-url','get-url','exec','file','text'], "Select the kind of application. HTTP, local program or static text. In the expression you can use the following variables: %p-phone number<br>%P-short number<br>%a-all words in SMS<br>%k-keyword<br>%1-first word in SMS,%2- ..<br>%i-smsc-id<br>%d-dlr result<br>%I-dlr id"],
	  ['get-url'			,'get-url' , ['string','',65], "The URL to fetch for response content. No parameter substitution is done, but X-Mbuni headers are sent as part of the HTTP request."],
	  ['post-url'			,'post-url', ['string','',65], "Response content is obtained as result of sending a HTTP POST request to the provided URL. The POST message is always  submitted using the multipart/form-data encoding (such as that used by a web browser when an HTML form has the enctype=multipart/form-data  parameter set). If http-post-parameters field is given (see below), then the relevant parameters are sent as part of the request. X-Mbuni headers are sent as well"],
	  ['http-post-parameters'	,'post-url', ['string','',65], "Used in conjunction with post-url. Parameters are provided in the same way as would be provided in an HTTP GET request (e.g. message=true&myname=test&image=%i)."],
	  ['exec'                       ,'exec'    , ['string','',65], "Program to execute to obtain response content. Response is exptected on standard output, and must be of type SMIL."],
	  ['file'			,'file'    , ['string','',65], "File from which to obtain response content. Response type is inferred from the file name extension for a few well-known media types (text, SMIL, GIF, JPEG, PNG, BMP/WBMP, WAV, AMR, MP3, etc.), otherwise it defaults to application/octet-stream."],
	  ['text'			,'text'    , ['string','',65], "If provided, the response content is the value of this parameter, and is assumed to be of media type text/plain."],
	  ['accept-x-mbuni-headers'     ,''        , ['bool','false'], "Set this to true if Mbuni should honour X-Mbuni HTTP headers in the service response. "],
	  ['service-code'		,''        , ['string','',30], "If set, Mbuni will use this as the ServiceCode parameter to send back the MMSC (MM7/SOAP only) in a SubmitReq packet. This paramter overrides the X-Mbuni-ServiceCode header, if set int the response."],
	  ['accepted-mmscs'		,''        , ['mselect','',3,0,'&list_mmsc'], "Only messages from these MMSCs will be routed to this service on match. Leave this empty to allow all."],
	  ['denied-mmscs'	        ,''        , ['mselect','',3,0,'&list_mmsc'], "Messages from these MMSCs will never be routed to this service on match. Leave this empty to allow all."],
	  ['pass-thro-headers' 		,''        , ['string','',20], "Set this to a comma-separated list of HTTP response headers that, if received from the service response, should be passed to the MMC (with their corresponding values) unchanged. Note that these headers are merged with other headers generated by Mbuni."],
	  ['faked-sender'		,''        , ['string','',10], "If set, Mbuni will change the sender address in the response message to this value, irrespective of any X-Mbuni headers or the original recipient address."],
	  ['catch-all'			,''        , ['bool','false'], "Set this service as default. Catch the message even if keyword is missing or wrong."],
	  ['assume-plain-text'		,''        , ['bool','false'], "Set this to true if an unknown content type in the response should be mapped to plain text."],
	  ['suppress-reply'		,''        , ['bool','false'], "Set this to true if Mbuni not send a reply back to the user of this service. Note that the request URL will still be invoked."],
	  ['strip-keyword'		,''        , ['bool','true'],  "Remove the first word (keyword) from message, before calling the application."],
	  ['omit-empty'		        ,''        , ['bool','false'], "Set this to true if Mbuni should send an empty response to the requestor if one is returned by the service."],
	  ['system-manager'		,''        , ['bool','false'], "If true, this service may only be used by system managers. (See the system manager list under smsbox configuration)"],
	  ['http-throttle'  		,''        , ['number',''],    "Max number of requests per sec. for this service only. (Only used if the application can't"],
	  ['override-subject'		,''			,['string','',40], "A string which is inserted in place of the subject when dispatching this service"],
	  ['allowed-receiver-regex'	,''	   , ['string', '', 40], "A regular expression, using POSIX Extended Regular Expression syntax. The search is case insentive. If set, it must match the receiver of an MMS for this service to be matched to the MMS."], 
	  ['allowed-sender-regex'	,''	   , ['string', '', 40], "A regular expression, using POSIX Extended Regular Expression syntax. The search is case insentive. If set, it must match the sender of an MMS for this service to be matched to the MMS."], 
	],
    'send-mms-user' =>
        [ ['group'      		,''        , ['readonly','send-mms-user',0,1]],
	  ['username'			,''        , ['string','',20,1], "Mandatory Username"],
	  ['password'			,''        , ['string','',20,1], "password"],
	  ['faked-sender'		,''        , ['string','',10],   "Optional sender address used (irrespective of whether from address has been specified in MMS send request)."],
	  ['delivery-report-url'	,''        , ['string','',60],   "Optional URL to call to send message delivery status as reported to the VAS gateway by a MMSC for a message submitted by this user."],
	  ['read-report-url'		,''        , ['string','',60],   "Optional URL to call to send message read status reports as returned to the VAS gateway by a MMSC for a message submitted by this user."],
	  ],
    'substitution-pattern' =>
	[	['group'			,''	, ['readonly','substitution-pattern',0,1]],
		['id'				,''	, ['string','',20,1], 'A unique name for this substitution pattern. This name is used to refer to this pattern from the mmsc configuration via either incoming-substitution-pattern or outgoing-substitution-pattern'],
		['match-pattern'		,''	, ['string','',40,1], 'A regular expression to match a particular type of number. Part enclosed in parenthesis can be referenced using $1..$n in the replacement string'],
		['replacement'			,''	, ['string','',40,1], 'A string which is inserted in place of the match-pattern. It is possible to make backreferences to the match-pattern using $1..$n, where $n is the nth parenthesis part of the match-pattern'],
	] 
};

#--------------------------------------------------------------------
# This function does it all ... called from the bottom of this script
sub do_request
{
    # initialize all global vars.
    init();

    # read the "mbuni" definition file.
    read_def();

    # add current instance name, and posible digest to "pname"
    $pname .= "?instance=$instance";
    $pname .= "&digest=$digest" if (length($digest));

    # read the selected instance config file
    $config_file = "$def{MBUNI_HOME}/etc/mbuni.$instance.conf";

    read_config(\%config);
    do_runmode();
}

#--------------------------------------------------------------------
# Initalize some globals
sub init
{
    my @tmp = ();

    $q = new CGI; # Get the request handler

    foreach $var ($q->param()) {
        @tmp = $q->param($var);
	$cgi_params{$var} = join(';', @tmp);
    }

    # Set up an alarm system to prevent the browser from timing out.
    # (used in the statistical output, which can be very slow)
    $timeout = 30;
    $SIG{ALRM} = sub { print ".\n"; alarm($timeout) };

    @instances = ();
    $instance = "";
    %config = ();
    %def = ();

    # setup the system global definition file.
    $def_file = "/etc/mbuni.def";

    $rm       = defined($cgi_params{rm}) ? $cgi_params{rm} : (defined($cgi_params{lrm}) ? $cgi_params{lrm} : "home");
    $lrm      = $rm;
    $digest   = $cgi_params{digest};
    $instance = $cgi_params{instance};
    $pname    = File::Basename::fileparse($0);
}

#--------------------------------------------------------------------
# Read the "/etc/mbuni.def" file to determin where kannel
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

    # Set default instance, if no one selected (first time)
    if ($instance eq "") {
	($instance) = split(/\,/,$def{INSTANCES});
    }

    # setup the @instances list for template header
    foreach (split(/\,/,$def{INSTANCES})) {
	my %row = (name => $_);
	$row{selected} = 1 if ($_ =~ /^$instance$/);
	push(@instances, \%row);
    }
}



#-------------------------------------------------------------------
# Now do the damed thing !!
sub do_runmode
{


#-------------------------------------------------------------------
# The "rm" cgi variable determins on which web page we are
#
# First the "homepage", the page that are shown if nothing else
# is specified.
#-------------------------------------------------------------------
if ($rm eq "home")
{
    my $tmpl = open_template("home.tmpl");

    writepage($tmpl);
}
#-------------------------------------------------------------------
# The top status page
#-------------------------------------------------------------------
elsif ($rm eq "status")
{
    my $tmpl = open_template("html.tmpl");

    writepage($tmpl);
}
#-------------------------------------------------------------------
# The main status page
#-------------------------------------------------------------------
elsif($rm eq "mainstatus")
{
	my $url;
	my $output;	
	my $tmpl = open_template("html.tmpl");
	
	my $pid = `ps -ef |grep mmsbox |grep $instance |grep -v grep |grep -v keepalive|cut -d' ' -f3`;
    chomp($pid);

    if ($pid ne "") {
    	$output = "Instance is running as PID $pid<br>";
		$url = "http://localhost:" . $config{'mbuni'}{0}{'admin-port'} . "/status.html";
		$output .=  wget($url);
    } else {
    	$output = "Instance is NOT running<br>";
    }

    $tmpl->param(html => $output);
    $tmpl->param(status => "1");
    writepage($tmpl);
}

elsif ($rm eq "mainstatus-old")
{
    my $tmpl = open_template("html.tmpl");

    my $pid = `ps -ef |grep mmsbox |grep $instance |grep -v grep |grep -v keepalive|cut -d' ' -f3`;
    chomp($pid);
    my $recv = `grep 'Received MMS' /home/realtime/mbuni/log/access.$instance.log | wc -l`;
    my $sent = `grep 'Sent MMS' /home/realtime/mbuni/log/access.$instance.log | wc -l`;
    
    #$out =~ /\<body\>(.*)\<\/body\>/sg;
    my $output = $1;

    if ($pid ne "") {	
    	$output = "Instance is running as PID $pid<br>";
    } else {
    	$output = "Instance is NOT running<br>";
    }
    $output=$output."<br><b>Received:</b>".$recv; 	    
    $output=$output."<br><b>Sent:</b>".$sent; 	    
 
    $tmpl->param(html    => $output);
    $tmpl->param(status  => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
# Graph
#-------------------------------------------------------------------
elsif ($rm eq "graph")
{
    my $tmpl = open_template("graph.tmpl");
    my $cmd;
    my $rrdtool='/usr/local/bin/rrdtool';

    $cmd = "$rrdtool graph $def{MBUNI_PROD}/cgi/png/$instance-hour.png ".
           "-s -7200 ".
	   "-w 600 ".
           "--title=\"$instance (2 hours)\" ".
           "DEF:sent=$def{MBUNI_HOME}/rrd/$instance.rrd:sent:AVERAGE ".
           "DEF:recv=$def{MBUNI_HOME}/rrd/$instance.rrd:recv:AVERAGE ".
           "DEF:queue=$def{MBUNI_HOME}/rrd/$instance.rrd:queue:AVERAGE ".
           "LINE2:sent#0000ff:Sent ".
           "LINE2:recv#33ff33:Recv ".
           "LINE2:queue#ff0000:Queue 2>&1 > /dev/null";
    system($cmd);

    $cmd = "$rrdtool graph $def{MBUNI_PROD}/cgi/png/$instance-day.png ".
           "-s -172800 ".
	   "-w 600 ".
           "--title=\"$instance (2 days)\" ".
           "DEF:sent=$def{MBUNI_HOME}/rrd/$instance.rrd:sent:AVERAGE ".
           "DEF:recv=$def{MBUNI_HOME}/rrd/$instance.rrd:recv:AVERAGE ".
           "DEF:queue=$def{MBUNI_HOME}/rrd/$instance.rrd:queue:AVERAGE ".
           "LINE2:sent#0000ff:Sent ".
           "LINE2:recv#33ff33:Recv ".
           "LINE2:queue#ff0000:Queue 2>&1 > /dev/null";
    system($cmd);

    $cmd = "$rrdtool graph $def{MBUNI_PROD}/cgi/png/$instance-month.png ".
           "-s -2678400 ".
	   "-w 600 ".
           "--title=\"$instance (1 month)\" ".
           "DEF:sent=$def{MBUNI_HOME}/rrd/$instance.rrd:sent:AVERAGE ".
           "DEF:recv=$def{MBUNI_HOME}/rrd/$instance.rrd:recv:AVERAGE ".
           "DEF:queue=$def{MBUNI_HOME}/rrd/$instance.rrd:queue:AVERAGE ".
           "LINE2:sent#0000ff:Sent ".
           "LINE2:recv#33ff33:Recv ".
           "LINE2:queue#ff0000:Queue 2>&1 > /dev/null";
    system($cmd);

    $cmd = "$rrdtool graph $def{MBUNI_PROD}/cgi/png/$instance-year.png ".
           "-s -31536000 ".
	   "-w 600 ".
           "--title=\"$instance (1 year)\" ".
           "DEF:sent=$def{MBUNI_HOME}/rrd/$instance.rrd:sent:AVERAGE ".
           "DEF:recv=$def{MBUNI_HOME}/rrd/$instance.rrd:recv:AVERAGE ".
           "DEF:queue=$def{MBUNI_HOME}/rrd/$instance.rrd:queue:AVERAGE ".
           "LINE2:sent#0000ff:Sent ".
           "LINE2:recv#33ff33:Recv ".
           "LINE2:queue#ff0000:Queue 2>&1 > /dev/null";
    system($cmd);

    #$tmpl->param(refresh => "1");
    $tmpl->param(status  => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
# The DLR status page
#-------------------------------------------------------------------
elsif ($rm eq "dlrstatus")
{
    my $tmpl = open_template("html.tmpl");

    my $out = wget("http://localhost:".$config{'core'}{0}{'admin-port'}."/status-dlr.html");

    $out =~ /\<body\>(.*)\<\/body\>/sg;
    my $output = "<center>" . $1 . "</center>";

    $tmpl->param(html => $output);
    $tmpl->param(status => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
# The documentation page
#-------------------------------------------------------------------
elsif ($rm eq "documentation")
{
    my $tmpl = open_template("html.tmpl");

    writepage($tmpl);
}
#-------------------------------------------------------------------
elsif ($rm eq "doc-admin") 
{
    my $tmpl = open_template("documentation.tmpl");
    my $html;

    open(FH, "<admin.html") || ERROR("Can't open file admin.html");
    my @lines = <FH>;
    close(FH);

    foreach (@lines) {
	$html .= $_;
    }

    $html =~ /<body[^>]*>(.*)<\/body/sg;
    my $output = $1;

    $tmpl->param(html => $output);
    $tmpl->param(documentation => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
elsif ($rm eq "doc-user")
{
    my $tmpl = open_template("documentation.tmpl");
    my $html;

    open(FH, "<user.html") || ERROR("Can't open file user.html");
    my @lines = <FH>;
    close(FH);

    foreach (@lines) {
	$html .= $_;
    }

    $html =~ /<body[^>]*>(.*)<\/body/sg;
    my $output = $1;

    $tmpl->param(html => $output);
    $tmpl->param(documentation => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
elsif ($rm eq "doc-download")
{
    my $tmpl = open_template("download.tmpl");

    $tmpl->param(documentation => "1");
    writepage($tmpl);
}
#-------------------------------------------------------------------
# The statistics page
#-------------------------------------------------------------------
elsif ($rm eq "statistics")
{
    my $tmpl = open_template("statistics.tmpl");
    my (@services, @smscs, @lacs);
    my @tmp = ();

    @tmp = list_services();
    foreach (sort @tmp) {
	my %row = (name => $_);
	push(@services, \%row);
    }

    @tmp = list_smsc_nobilling();
    foreach (sort @tmp) {
	my %row = (name => $_);
	push(@smscs, \%row);
    }
    
    @tmp = list_shortnumber();
    foreach (sort @tmp) {
	my %row = (name => $_);
	push(@lacs, \%row);
    }

    $tmpl->param(startdate => s_e_date(0));
    $tmpl->param(enddate   => s_e_date(0));
    $tmpl->param(services  => \@services);
    $tmpl->param(smscs     => \@smscs);
    $tmpl->param(lacs      => \@lacs);

    writepage($tmpl);
}
#-------------------------------------------------------------------
# The statistics output page
#-------------------------------------------------------------------
elsif ($rm eq "statsubmit")
{
    my $startdate = $cgi_params{startdate};
    my $enddate   = $cgi_params{enddate};
    my $services  = $cgi_params{services};
    my $msisdn    = $cgi_params{msisdn};
    my $search    = $cgi_params{search};
    my $smscs     = $cgi_params{smscs};
    my $sent      = $cgi_params{sent};
    my $modtaget  = $cgi_params{modtaget};
    my $fejlet    = $cgi_params{fejlet};
    my $afvist    = $cgi_params{afvist};
    my $dlr       = $cgi_params{dlr};
    my $dlrmask   = $cgi_params{dlrmask};
    my $total     = $cgi_params{total};
    my $subtotal  = $cgi_params{subtotal};
    my $msg       = $cgi_params{msg};
    my $format    = $cgi_params{format};
    my $lac       = $cgi_params{lac};
    my $orderby   = $cgi_params{orderby};
    my $submit    = $cgi_params{submit};

    my $kstat   = "$def{MBUNI_PROD}/bin/kstat";

    my $cmd = "$kstat";
    my @out;
    my @show = ();

    # Now lets check the parameters ...
    ERROR("Wrong startdate format") unless ($startdate =~ /^200\d-[01]\d-[0-3]\d$/);
    ERROR("Wrong enddate format")   unless ($enddate   =~ /^200\d-[01]\d-[0-3]\d$/);

    # Build the commandline for "kstat"
    $cmd .= " --instance $instance";
    $cmd .= " --format $format"          if ($format ne "");
    $cmd .= " --startdate $startdate"    if ($startdate ne "");
    $cmd .= " --enddate $enddate"        if ($enddate ne "");
    $cmd .= " --services $services"      if ($services ne "");
    $cmd .= " --smscs '$smscs'"          if ($smscs ne "ALL");
    $cmd .= " --msisdn '$msisdn'"        if ($msisdn ne "");
    $cmd .= " --search '$search'"        if ($search ne "");
    $cmd .= " --lacs '$lac'"             if ($lac ne "ALL");
    $cmd .= " --dlrmask"                 if ($dlrmask);
    $cmd .= " --orderby $orderby"        if ($orderby);
    $cmd .= " --total"                   if ($total);
    $cmd .= " --subtotal $subtotal"      if ($subtotal);
    $cmd .= " --msg"                     if ($msg);

    push(@show,"SENT")       if ($sent);
    push(@show,"RECEIVE")    if ($modtaget);
    push(@show,"FAILED")     if ($fejlet);
    push(@show,"DISCARDED")  if ($afvist);
    push(@show,"DLR")        if ($dlr);
	push(@show,"DISPATCHED") if ($videresent);

    $cmd .= " --show ".join(',',@show)   if ($show[0] ne "");

	$cmd .= " --log-format=mbuni ";
	

    my $tmpl = open_template("header.tmpl");
    $tmpl->param(statistics => "1");
    $tmpl->param(lrm      => $lrm);
    $tmpl->param(nodename   => hostname());
    $tmpl->param(pname      => $pname);
    $tmpl->param(instances  => \@instances);
    $tmpl->param(instance   => $instance);
    $tmpl->param(digest     => $digest);
    
    $| = 1;  # skip the output buffer
    print $q->header(-type=>'text/html', -timeout=>'600');
    print $tmpl->output;

    print "<!--\n";
    print "$cmd\n";
    alarm($timeout);

    # Get the statistical data
    open(PIPE, "$cmd 2>&1 |");
    while (<PIPE>) { 
	if ($timeout) {
	    alarm(0);
	    print "-->\n";
	    $timeout = 0;
	    print "<pre>\n" unless ($format eq "html");
	}
	print $_; 
    }
    close(PIPE);

    print "</pre>\n" unless ($format eq "html");
    $tmpl = open_template("footer.tmpl");
    $tmpl->param($rm => "1");
    $tmpl->param(nodename => hostname());
    print $tmpl->output;
    exit;
}
#-------------------------------------------------------------------
# The configuration page
# Show the configuration "menu"
#-------------------------------------------------------------------
elsif ($rm eq "configuration")
{
    my $tmpl = open_template("configuration.tmpl");

    login();
    writepage($tmpl);
}
#-------------------------------------------------------------------
# The control page
# Show the control "menu"
#-------------------------------------------------------------------
elsif ($rm eq "control")
{
    my $tmpl = open_template("control.tmpl");

    login();
    writepage($tmpl);
}
#-------------------------------------------------------------------
# Stop kannel, using the init.d script
#-------------------------------------------------------------------
elsif ($rm eq "stop")
{
    $lrm = "control";
    command("$def{MBUNI_PROD}/bin/mbunictl stop $instance");
}
#-------------------------------------------------------------------
# Start kannel, using the init.d script
#-------------------------------------------------------------------
elsif ($rm eq "start")
{
    $lrm = "control";
    command("$def{MBUNI_PROD}/bin/mbunictl start $instance");
}
#-------------------------------------------------------------------
# Restart kannel, using the init.d script
#-------------------------------------------------------------------
elsif ($rm eq "restart")
{
    $lrm = "control";
    command("$def{MBUNI_PROD}/bin/mbunictl restart $instance");
}
#-------------------------------------------------------------------
# Flush a queue, using admin interface
#-------------------------------------------------------------------
elsif ($rm =~ /^flush-(incoming|outgoing|dlr)-queue/)
{
	
	my $url;
	my $queue = $1;
	my $tmpl = open_template("output.tmpl");
		
	$lrm = "control";
	
	$url = "http://localhost:" . $config{'mbuni'}{0}{'admin-port'};
	$url .= "/flush-$queue?password=";
	$url .= $config{'mbuni'}{0}{'admin-password'};

    $tmpl->param(output => wget($url));
    $tmpl->param(control => "1");
    writepage($tmpl);
}

#-------------------------------------------------------------------
#
#-------------------------------------------------------------------
elsif ($rm eq "sendsms")
{
    my $tmpl = open_template("sendsms.tmpl");
    my (@smscs, @lacs, @tcs);
    my @tmp = ();

    @tmp = list_tariff();
    foreach (@tmp) {
	my %row = (name => $_);
	push(@tcs, \%row);
    }

    $tmpl->param(tcs     => \@tcs);
    $tmpl->param(control => "1");

    writepage($tmpl);
}
#-------------------------------------------------------------------
#
#-------------------------------------------------------------------
elsif ($rm eq "sendsmssubmit")
{
    my $tmpl        = open_template("output.tmpl");
    my $lac         = $cgi_params{lac};
    my $smsc        = $cgi_params{smsc};
    my $msisdn      = $cgi_params{msisdn};
    my $validity    = $cgi_params{validity};
    my $message     = $cgi_params{message};
    my $udh         = $cgi_params{udh};
    my $tc          = $cgi_params{tc};
    my $charset     = $cgi_params{charset};
    my $messagetype = $cgi_params{messagetype};
    my ($out, $url);

    $msisdn =~ s/[\r\n]//g;
    $message = urlencode($message);

    $url  = "http://localhost:" . $config{'smsbox'}{0}{'sendsms-port'} . "/sendsms?";
    $url .= "&user=" . $config{'sendsms-user'}{0}{'username'};
    $url .= "&pass=" . $config{'sendsms-user'}{0}{'password'};
    $url .= "&from=$lac&to=$msisdn";
    $url .= "&tariff_class=$tc" if ($tc ne "none");
    $url .= "&smsc=$smsc";
    $url .= "&messagetype=$messagetype&text=$message";
    $url .= "&validity=$validity" if ($validity ne "0");
    $url .= "&udh=$udh" if ($messagetype eq "binary");
    $url .= "&charset=$charset" if ($charset ne "");

    $out = wget($url);

    $tmpl->param(control => "1");
    $tmpl->param(output  => $out);
    writepage($tmpl);
}
#-------------------------------------------------------------------
# Edit the "core" section of mbuni configuration
#-------------------------------------------------------------------
elsif ($rm eq "editcore")
{
    form_edit('core', \%config);
}
#-------------------------------------------------------------------
# Edit the "mbuni" section of mbuni configuration
#-------------------------------------------------------------------
elsif ($rm eq "editmbuni")
{
    form_edit('mbuni', \%config);
}
#-------------------------------------------------------------------
# Edit the "smsc"'s sections of mbuni configuration file
#-------------------------------------------------------------------
elsif ($rm eq "editmmsc")
{
    if (defined($cgi_params{nr}))
    {
        form_edit('mmsc', \%config, $cgi_params{nr});
    }
    else
    {
	my $tmpl = open_template("list.tmpl");
	my @list;
	my %par;
	my $nr;
        my $color = 1;

	foreach $nr (sort {$a <=> $b} keys %{$config{'mmsc'}} )
	{
	    my %par;

	    $par{nr}    = $nr;
	    $par{text1} = $config{'mmsc'}{$nr}{'id'};
	    $par{text2} = $config{'mmsc'}{$nr}{'mmsc'};
	    $par{text3} = $config{'mmsc'}{$nr}{'my-number'};
	    $par{elink} = "$pname&rm=list&action=edit&type=mmsc&nr=$nr";
	    $par{dlink} = "$pname&rm=list&action=delete&type=mmsc&nr=$nr";
	    $par{clink} = "$pname&rm=list&action=copy&type=mmsc&nr=$nr";
	    $par{tlink} = "$pname&rm=list&action=toggle&type=mmsc&nr=$nr";
	    $par{color} = $color; $color=($color ? 0: 1);
	    $par{disabled} = $config{'mmsc'}{$nr}{'disabled'};
	    push(@list, \%par);
	}

	$tmpl->param(nlink   => "$pname&rm=list&action=new&type=mmsc");
	$tmpl->param(header1 => "id");
	$tmpl->param(header2 => "mmsc");
	$tmpl->param(header3 => "short number");
        $tmpl->param(list => \@list);
	$tmpl->param(type => "mmsc");
        $tmpl->param(configuration => "1");

	writepage($tmpl);
    }
}
#-------------------------------------------------------------------
# Edit the "mms-service"'s sections of mbuni's configuration file
#-------------------------------------------------------------------
elsif ($rm eq "editservices")
{
    # if "nr" is defined, edit the specified "service"
    # else present a list of "keywords"

    if (defined($cgi_params{nr}))
    {
        form_edit('mms-service', \%config, $cgi_params{nr});
    }
    else
    {
	my $tmpl = open_template("list.tmpl");
	my @list;
	my $nr;
        my $color = 1;
	my $def_nr = -1;

	{
	    my %par;
	    $par{separator} = "Keyword services";
	    push(@list, \%par);
	}

	# push all keywords to the list, ordered by name
	foreach $nr (sort { uc($config{'mms-service'}{$a}{'name'}) cmp  uc($config{'mms-service'}{$b}{'name'}) } keys %{$config{'mms-service'}} )
	{
	    next unless (length($config{'mms-service'}{$nr}{'keyword'}));
	    if ($config{'mms-service'}{$nr}{'keyword'} eq "default") {
		$def_nr = $nr;
		next;
	    }

	    add_to_list($nr, \@list, \$color);
	}

	{
	    my %par;
	    $par{separator} = "Regular expression services";
	    push(@list, \%par);
	}

	$color = 1;

	# push all regular expressions to the list, original order
	foreach $nr (sort { $a <=> $b } keys %{$config{'mms-service'}} )
	{
	    next if (length($config{'mms-service'}{$nr}{'keyword'}));

	    add_to_list($nr, \@list, \$color);
	}

	{
	    my %par;
	    $par{separator} = "Default service";
	    push(@list, \%par);
	}

	$color = 1;

	# push the default service to the list
	if ($def_nr != -1) {
	    add_to_list($def_nr, \@list, \$color);
	}

	$tmpl->param(nlink   => "$pname&rm=list&action=new&type=mms-service");
	$tmpl->param(header1 => "service name");
	$tmpl->param(header2 => "keyword/regex");
	$tmpl->param(header3 => "get-url");
	$tmpl->param(list => \@list);
	$tmpl->param(configuration => "1");
	$tmpl->param(type => "mms-service");
	$tmpl->param(note => "1");
	writepage($tmpl);
    }
}
#-------------------------------------------------------------------
elsif ($rm eq "editusers")
{
    if (defined($cgi_params{nr}))
    {
	form_edit('send-mms-user', \%config, $cgi_params{nr});
    }
    else
    {
	my $tmpl = open_template("list.tmpl");
	my @list;
	my %par;
	my $nr;
        my $color = 1;

	foreach $nr (keys %{$config{'send-mms-user'}} )
	{
	    my %par;

	    $par{nr} = $nr;
	    $par{text1} = $config{'send-mms-user'}{$nr}{'name'};
	    $par{text2} = $config{'send-mms-user'}{$nr}{'username'};
	    $par{text3} = $config{'send-mms-user'}{$nr}{'password'};
	    $par{elink} = "$pname&rm=list&action=edit&type=send-mms-user&nr=$nr";
	    $par{dlink} = "$pname&rm=list&action=delete&type=send-mms-user&nr=$nr";
	    $par{clink} = "$pname&rm=list&action=copy&type=send-mms-user&nr=$nr";
	    $par{tlink} = "$pname&rm=list&action=toggle&type=send-mms-user&nr=$nr";
	    $par{color} = $color; $color=($color ? 0: 1);
	    $par{disabled} = $config{'sendsms-user'}{$nr}{'disabled'};
	    push(@list, \%par);
	}

	$tmpl->param(nlink   => "$pname&rm=list&action=new&type=send-mms-user");
	$tmpl->param(header1 => "service name");
	$tmpl->param(header2 => "username");
	$tmpl->param(header3 => "password");
        $tmpl->param(list => \@list);
	$tmpl->param(type => "send-mms-user");
        $tmpl->param(configuration => "1");

	writepage($tmpl);
    }
}
elsif($rm eq "editpatterns")
{
	my $tmpl = open_template("list.tmpl");
	my @list;
	my $nr;
	my $color = 1;

	foreach $nr (keys %{$config{'substitution-pattern'}})
	{
		my %par;
		
		$par{nr} = $nr;
		$par{text1} = $config{'substitution-pattern'}{$nr}{'id'};
		$par{text2} = $config{'substitution-pattern'}{$nr}{'match-pattern'};
		$par{text3} = $config{'substitution-pattern'}{$nr}{'replacement'};
		$par{elink} = "$pname&rm=list&action=edit&type=substitution-pattern&nr=$nr";
		$par{dlink} = "$pname&rm=list&action=delete&type=substitution-pattern&nr=$nr";
		$par{clink} = "$pname&rm=list&action=copy&type=substitution-pattern&nr=$nr";
		$par{tlink} = "$pname&rm=list&action=toggle&type=substitution-pattern&nr=$nr";
		$par{color} = $color; $color=($color ? 0: 1);
		$par{disabled} = $config{'substitution-pattern'}{$nr}{'disabled'};
		push(@list, \%par);
	}

        $tmpl->param(nlink   => "$pname&rm=list&action=new&type=substitution-pattern");
        $tmpl->param(header1 => "id");
        $tmpl->param(header2 => "match-pattern");
        $tmpl->param(header3 => "replacement");
        $tmpl->param(list => \@list);
        $tmpl->param(type => "substitution-pattern");
        $tmpl->param(configuration => "1");

	writepage($tmpl);
}

#-------------------------------------------------------------------
elsif ($rm eq "managebackups")
{
	$action = $cgi_params{action} || "no action";
	my $backup = $cgi_params{name};
    my $tmpl = open_template("backup.tmpl");	
	my $success = 1;
	my $msg = "";
    
	if ($action eq "new") {
		$tmpl->param(newbackup => "1");
	} elsif ($action eq "create") {
	    $success, $msg = create_backup($backup);
    } elsif ($action eq "restore") {
	   $success, $msg = restore_backup($backup);
    } elsif ($action eq "delete") {
       unlink("$def{MBUNI_HOME}/tmp/mbuni.$instance.conf.$backup");
       if ( -f "$def{MBUNI_HOME}/tmp/mbuni.$instance.conf.$backup") {
       		$success = 0; $msg = "Could not delete backup: $backup";
       } else {
       		$success = 1; $msg = "Deleted backup: $backup";
       }
    }
 
	if ($success) {
		$tmpl->param(notice => $msg);
	} else {
		$tmpl->param(errormsg => $msg);
	}

    my @list;
    # Create a list of existing backups
    my @files = glob("$def{MBUNI_HOME}/tmp/mbuni.$instance.conf.*"); 
        
	my @files_sorted_by_time = reverse sort { (stat($a))[10] <=> (stat($b))[10] } @files;
        
    foreach $backupfile (@files_sorted_by_time) {
	    my %par;
        my $fname = basename($backupfile);
        my $name = $1 if $backupfile =~ /mbuni\.$instance\.conf\.(.*)/;
		my $backupdate = (stat($backupfile))[10];
		
        $par{name} = $name;
		$par{date} = scalar localtime($backupdate);
        $par{restorelink} = "$pname&rm=managebackups&action=restore&name=$name";
        $par{deletelink} = "$pname&rm=managebackups&action=delete&name=$name";
        $par{color} = $color;
        $color=($color ? 0: 1);
        push(@list, \%par);
    }
        
    #$tmpl->param(name => "configuration");
    $tmpl->param(managebackups => "1");
    $tmpl->param(instance => $instance);
    $tmpl->param(action => $action);
    $tmpl->param(nlink => "$pname&rm=managebackups&action=new");
    $tmpl->param(list => \@list);
    $tmpl->param(control => "1");

    writepage($tmpl);
}

#-------------------------------------------------------------------
elsif ($rm eq "list")
{
    my $nr     = $cgi_params{nr};
    my $type   = $cgi_params{type};
    my $action = $cgi_params{action};

    if ($action eq "edit") {

        form_edit($type, \%config, $nr);

    } elsif ($action eq "new") {

	# set nr to the next available
	foreach (sort {$a <=> $b} keys %{$config{$type}}) {
	    $nr = $_;
	}
	$nr++;
	form_edit($type, \%config, $nr);

    } elsif ($action eq "copy") {

	my $nnr = $nr+0.5;
	foreach $var (keys %{$config{$type}{$nr}}) {
	    $config{$type}{$nnr} = $config{$type}{$nr};
	}
	write_config(\%config);

    } elsif ($action eq "delete") {

	delete($config{$type}{$nr});
	write_config(\%config);

    } elsif ($action eq "toggle") {
	$config{$type}{$nr}{disabled} = ($config{$type}{$nr}{disabled} ? 0 : 1);
	write_config(\%config);
    }

    if ($type =~ /mms-service/) {
	print $q->redirect("$pname&rm=editservices");
    } elsif ($type =~ /mmsc/) {
	print $q->redirect("$pname&rm=editmmsc");
    } elsif ($type =~ /send-mms-user/) {
	print $q->redirect("$pname&rm=editusers");
	} elsif ($type =~ /substitution-pattern/) {
	print $q->redirect("$pname&rm=editpatterns");
    } else {
	print $q->redirect("$pname");
    }
    exit;
}
#-------------------------------------------------------------------
elsif ($rm eq "save")
{
    my ($row,$man,$x, $opt);
    my ($param_name, $frmctrl, $opt, $help);
    my $group = $cgi_params{type};
    my $nr    = $cgi_params{nr};
    my @control_values = ();


    # delete entire group(nr) (restore disabled flag)
    my $disabled = $config{$group}{$nr}{disabled};
    delete $config{$group}{$nr};
    $config{$group}{$nr}{disabled} = $disabled;

    # Go through all cgi variables returned from the browser
    # (We have to go through several times, because we can't control the order
    #  in which the cgi-vars comes :-)

    # First round: Register the value off all control elements
    foreach $cgi_var (keys %cgi_params) {
	next if ($cgi_var !~ /^(ctrl_|hctrl_)/);
	push(@control_values, $cgi_params{$cgi_var});
    }

    # Second round: - Find each element in %forms, see if it's mandatory etc.
    #               - Skip elements not found in %forms
    #               - Delete parameters from %config, that's not active anymore
    #               - Save the value in %config
    foreach $cgi_var (keys %cgi_params) {

	$cgi_ctrl = '';
	$var =  $cgi_var;
	# check if it contains an underscore "_"
	if ($cgi_var =~ /_/) {
	    ($cgi_ctrl, $var) = split(/_/, $cgi_var);
	}

	# See if we can find it in the "$forms" var. (in the specific $group)
	$found = 0;
	FORMS: foreach $row (@{$$forms{$group}}) {

	    # get the options from the row in question.
	    ($param_name, $frmctrl, $opt, $help) = @{$row};

	    # see if it matches
	    if (($var eq $param_name) && ($cgi_ctrl eq $frmctrl))
	    {
		# remember if it's mandatory.
		($x,$x,$x,$man, @list) = @{$opt};

		# check for controling elements
		if (($frmctrl eq "ctrl") || ($frmctrl eq "hctrl")) {
		    $found = 1;
		    last FORMS;
		}

		# check if this values isn't part of an active "control"
		if (length($frmctrl) && not grep(/$frmctrl/, @control_values)) {
		    #delete $config{$group}{$nr}{$var};
		    next;
		}

		# ok, we found it.
		$found = 1;
		last FORMS;
	    }
	}
	# skip cgi-vars not found in $forms
	next unless($found);

	# check if mandatory field is filled out
	if (defined($man) && ($man > 0) && (length($cgi_params{$cgi_var}) == 0)) {
	    ERROR("Mandatory field -$var- ($v) ($man) is missing");
	}

	# delete fields which isn't filled
	#if (!length($cgi_params{$cgi_var}) && length($config{$group}{$nr}{$var})) {
	    #delete $config{$group}{$nr}{$var};
	    #next;
	#}
	next unless (length($cgi_params{$cgi_var}));

	# save field in config var
        $config{$group}{$nr}{$var} = $cgi_params{$cgi_var};
        $config{$group}{$nr}{$var}{hctrl} = 1 if ($frmctrl eq "hctrl");
    }

    # Write the configuration file
    write_config(\%config);

    # Go back to last edit / configuration page
    if ($group eq "mms-service") {
	print $q->redirect("$pname&rm=editservices");
    } elsif ($group eq "mmsc") {
	print $q->redirect("$pname&rm=editmmsc");
    } elsif ($group eq "send-mms-user") {
	print $q->redirect("$pname&rm=editusers");
	} elsif ($group eq "substitution-pattern") {
	print $q->redirect("$pname&rm=editpatterns");
    } else {
	print $q->redirect("$pname&rm=configuration");
    }
    exit;
}
#-------------------------------------------------------------------
elsif ($rm =~ /^delete_/)
{
    $rm =~ s/delete_//;
    my ($group, $nr) = split(/-/, $rm);
    $group = fixunderscore($group);

    delete($config{$group}{$nr});

    # Write the new configuration file
    write_config(\%config);

    if ($group =~ /mms-service/)
    {
	print $q->redirect("$pname&rm=editservices");
    }
    elsif ($group =~ /mmsc/)
    {
	print $q->redirect("$pname&rm=editmmsc");
    }
    elsif ($group =~ /send-mms-user/)
    {
	print $q->redirect("$pname&rm=editusers");
    }
	elsif ($group =~ /substitution-pattern/)
	{
	print $q->redirect("$pname&rm=editpatterns")
	}
    else
    {
        print $q->redirect("$pname");
    }
    exit;
}
#-------------------------------------------------------------------
elsif ($rm eq "login")
{
    my $url;

    # encrypt the password, this will be the "digest" value
    $encrypt = crypt($digest, "RG");

    # fix pname (rm/digest already there)
    $pname =~ s/&digest=[^&]*//;
    $pname =~ s/&rm=[^&]*//;

    $url = "$pname&rm=" . $cgi_params{'lrm'};

    if ($encrypt eq $def{PASSWORD}) {
	$url = "$pname&rm=" . $cgi_params{'lrm'} . "&digest=$encrypt";
    }

    print $q->redirect($url);
}
#-------------------------------------------------------------------
else
{
    ERROR("Internal error or misuse of the application ($rm)($lrm)");
}

} 
#------------------------------------------------------------------
# end of do_runmode
#------------------------------------------------------------------


#------------------------------------------------------------------
# Print the edit form
# Args:
#	group name
#       \%config
#       number (if multi group), defaults to "0"
#------------------------------------------------------------------
sub form_edit
{
    my $group  = shift;
    my $config = shift;
    my $nr     = shift;
    my $tmpl = open_template("edit.tmpl");
    my @parameters;
    my (@list,@inner);
    my ($var, $type, $default, $size, $mandatory, $val);
    my %frmval = ();
    my $param;

    # set default value of "nr"
    $nr = 0 unless defined($nr);

    # Run through all possible parameters from "%forms"
    foreach $param (@{$$forms{$group} })
    {
	my %par;	# Allocate new parameter

	($var, $frmctrl, $options, $help) = @{$param};
	($type,$default,$size,$mandatory,@list) = @{$options};

	if (length($frmctrl)) {
	    $par{name}  = $frmctrl . "_" . $var; # set the parameter name
	} else {
	    $par{name}  = $var;	# set the parameter name
	}
	$par{$type} = 1;	# set the parameter type
	$par{text}  = $var;	# the input field "text"

	# Set the parameter value (default value if not set)
	$par{value} = $$config{$group}{$nr}{$var};
	$par{value} = $default unless (defined($par{value}));

	# fix 0/1 to false/true (for old boolean settings)
	if ($type =~ /bool/) {
	    $par{value} = "true" if ($par{value} eq '1');
	    $par{value} = "false" if ($par{value} eq '0');
	}

	# set true/false for boolean values
	$par{$par{value}} = 1 if ($type =~ /bool/);

	# set "size" of input field (default 15)
	$size = 15 unless defined($size);
	$par{size} = $size;

	# set "mandatory" field
	$par{mandatory} = 1 if (defined($mandatory) && $mandatory>0);

	# set list values
	if (defined(@list)) {
	    my @inner = ();
	    foreach $val (@list) {

		# check if 'val' is a function ref.
		if ($val =~ /^&.*/) {
		    $val =~ s/^&//;
		    my @sublist = &$val;
		    foreach $val (@sublist) {
			my %par2;
			$par2{option} = $val;
			$par2{selected} = 1 if ($$config{$group}{$nr}{$var} =~ /(^|;)$val(;|$)/);
			push(@inner, \%par2);
		    }

		} else {
		    my %par2;
		    $par2{option} = $val;
		    $par2{selected} = 1 if ($$config{$group}{$nr}{$var} =~ /(^|;)$val(;|$)/);
		    push(@inner, \%par2);
		}
	    }

	    $par{list} = \@inner;
	}

	# set the controls
	if ($frmctrl eq "ctrl" or  $frmctrl eq "hctrl") {
	    my $inner = $par{list};
	    my $js = "";
	    foreach (@{$inner}) {
		$js .= "DivOff(\'".$_->{option}."\');";
	    }
	    $js .= "DivOn(this.form.".$par{name}.".value)";
	    $par{ctrl} = $js;

	    # remember the value
	    $frmval{$par{value}} = 1;

	} elsif ($frmctrl ne "") {
	    $par{div} = $frmctrl;
	    $par{display} = "none";
	    $par{display} = "block" if ($frmval{$frmctrl});
	}

	# set the help text
	$par{help} = $help if (length($help));

	# put parameter on the list
	push(@parameters, \%par);
    }

    # Write all params to the HTML Template
    $tmpl->param(parameters    => \@parameters);
    $tmpl->param(rm          => "save");
    $tmpl->param(nr            => $nr);
    $tmpl->param(type          => $group);
    $tmpl->param(configuration => "1");

    writepage($tmpl);
}

#------------------------------------------------------------------
# Reads mbuni configuration file
#
# Arguments: \%config
#------------------------------------------------------------------
sub read_config
{
    my $config = shift;
    my $line;
    my $section;
    my $var;
    my $value;
    my $num;
    my $disabled;
    my $hctrl;

    # Open and read it
    open(FH, "<$config_file") || ERROR("Can't open '$config_file'");

    while ($line = <FH>)
    {
	# clear flags
	$disabled = 0;
	$hctrl = 0;

        # get rid of newline
        chomp($line);

        # skip comment lines (beginning with '#')
        #      blank lines
        next if ($line =~ /^#[^\&\*]/);
        next if (length($line) < 2);

        # split the line at the '=' sign
        ($var, $value) = split('=', $line, 2);

        # clean $var and $value for whitespace
        $var   =~ s/\s//g;
        $value = squeeze($value);

	# Check if group is disabled
	if ($var =~ /^#&/) {
	    $var =~ s/^#&//;
	    $disabled = 1;
	}

	# check if element is s hctrl type
	if ($var =~ /^#\*/) {
	    $var =~ s/^#\*//;
	    $hctrl = 1;
	}


        # Get section headers
        if ($var =~ /group/)
        {
            $section = $value;
	    $num = 0;
	    $num = scalar(keys %{ $$config{$section} }) if (defined($$config{$section}));
            next;
        }

        # fix "long" values, split over several lines
        while (($value =~ tr/"/"/) == 1)
        {
            die "Unexpected end of file. ($config_file)" if (not $line = <FH>);
            chomp($line);
            next if ($line =~ /^#[^&\*]/);
            $value .= $line;
        }
        
        # insert into $cfg
        $$config{$section}{$num}{$var} = $value;
	$$config{$section}{$num}{disabled} = $disabled;
	$$config{$section}{$num}{$var}{hctrl} = $hctrl;
    }

    close(FH);
}

#------------------------------------------------------------------
# Writes Kannel configuration file
#
# Arguments: \%config
#------------------------------------------------------------------
sub write_config
{
    my $config = shift;
    my ($group, $nr, $var, $value);

    # make a backup of the "kannel.conf" file
	create_backup();

    open(FH, ">$config_file") || ERROR("Cannot open $config_file");
    print FH "#This file is auto-generated from RealGate Admintool\n";
    print FH "#---------------------------------------------------\n";
    print FH "\n";

    foreach $group ('core','mmsc','mbuni','send-mms-user','mms-service', 'substitution-pattern')
    {
	print FH "#--------- $group_ ---------\n";
        foreach $nr (sort {$a <=> $b} keys %{ $$config{$group} })
	{
	    print FH "# $group group nr($nr)\n";
	    print FH "#&" if ($$config{$group}{$nr}{disabled});
	    print FH "group = $group\n";
	    foreach $var (sort keys %{ $$config{$group}{$nr} })
	    {
		next if ($var eq "disabled");
		print FH "#&" if ($$config{$group}{$nr}{disabled});
		print FH "#*" if ($$config{$group}{$nr}{$var}{hctrl});
	        $value = $$config{$group}{$nr}{$var};
		if ($value =~ / /) {
		    print FH "$var = \"$value\"\n";
		} else {
		    print FH "$var = $value\n";
		}
	    }
	    print FH "\n";
	}
	print FH "\n";
    }
    print FH "\n";
    close(FH);
}

sub create_backup
{
    my $name = shift || dato();

    if (-f "$def{MBUNI_HOME}/tmp/mbuni.$instance.conf.$name") {
        return 0, "File name allready exists!";
    } else {
        if (0 == system("cp $config_file $def{MBUNI_HOME}/tmp/mbuni.$instance.conf.$name")) {
			return 1, "Created backup: $name";
		} else {
			return 0,"create backup $name failed with error code $?";
		}
    }
}

sub restore_backup
{
	my $name = shift;
	my $backupfile = "$def{MBUNI_HOME}/tmp/mbuni.$instance.conf.$name";
	my $cfgfile = "$def{MBUNI_HOME}/etc/mbuni.$instance.conf";
	
	if (-f $backupfile) {
		system("cp $backupfile $cfgfile");
		return 1, "Restored backup: $name. Restart to activate changes.";
	} else {
		return 0, "No such backup!";
	}
}

#------------------------------------------------------------------
# ERROR: Writes an error message
#------------------------------------------------------------------
sub ERROR
{
    my $errmsg = shift;
    my $tmpl = open_template("error.tmpl");
    $tmpl->param(text => $errmsg);

    writepage($tmpl);
}

#-----------------------------------------------------------------
# open_template
#-----------------------------------------------------------------
sub open_template
{
    my $filename = shift;
    my $template = HTML::Template->new(filename          => $filename,
    				       path              => "templates",
				       die_on_bad_params => 0,
				       loop_context_vars => 1,
				       cache             => 1);
    return $template;
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

#----------------------------------------------------------
# Do a system command, show stdout from the command
# on a page.
#----------------------------------------------------------
sub command
{
    my $cmd = shift;
    my @out = popen($cmd) or ERROR("Failed to execute -$cmd-");
    my $output = "";

    foreach (@out)
    {
        $output .= $_;
    }

    my $tmpl = open_template("output.tmpl");
    $tmpl->param(output => $output);
    $tmpl->param(control => "1");

    writepage($tmpl);
}

#-----------------------------------------------------------
# Write the template-page and exit
#-----------------------------------------------------------
sub writepage
{
    my $tmpl = shift;

    $tmpl->param($rm     => "1");
    $tmpl->param(lrm      => $lrm);
    $tmpl->param(nodename  => hostname());
    $tmpl->param(pname     => $pname);
    $tmpl->param(instances => \@instances);
    $tmpl->param(instance  => $instance);
    $tmpl->param(digest    => $digest);
    print $q->header(-type=>'text/html', charset=>'utf8', -expires=>'-1d');
    print $tmpl->output;
    exit;
}
#-----------------------------------------------------------
# Execute system command, return output as "@out"
#-----------------------------------------------------------
sub popen
{   
    my $command = $_[0];
    my @output;

    open(PH, "$command 2>&1 |");
    @output = <PH>;
    close(PH);
    return @output;
}

#----------------------------------------------------------
#
#----------------------------------------------------------
sub LOG
{
    my $text = shift;

    open(FHLOG, ">>/tmp/admin.log");
    print FHLOG "$text\n";
    close(FHLOG);
}

#------------------------------------------------------------------
# Sub         : dato
# Returns     : date (yyyymmdd)
#------------------------------------------------------------------
sub dato
{
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst, $date);

    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
    $date = sprintf("%04d%02d%02d-%02d", $year+1900, $mon+1, $mday, $hour);

    return $date;
}

#------------------------------------------------------------------
# wget
#------------------------------------------------------------------
sub wget
{
    my $url = shift;

    my $ua = new LWP::UserAgent;
    $ua->agent("RealGate/1.0" . $ua->agent);

    my $req = HTTP::Request->new(GET => $url);
    $req->content_type('application/x-www-form-urlencoded');
    $req->content($url);
    $ua->timeout(30);
    my $res = $ua->request($req);

    return $res->content if ($res->is_success);

    return "Error in request: $url\nContent: " . $res->content;
}

#------------------------------------------------------------------
# urlencode
#------------------------------------------------------------------
sub urlencode
{
    my $str = shift;
    $str =~ s/([^\w_.-])/uc sprintf("%%%02x",ord($1))/eg;

    return $str;
}

#------------------------------------------------------------------
# s_e_date
#------------------------------------------------------------------
sub s_e_date
{
    my $day = shift;
    my $datetime;
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime();
    if ($day != 0) {
	$datetime = sprintf("%04d-%02d-%02d", $year+1900,$mon+1,$day);
    } else {
	$datetime = sprintf("%04d-%02d-%02d", $year+1900,$mon+1,$mday);
    }

    return $datetime;
}

#------------------------------------------------------------------
# list_smsc
#------------------------------------------------------------------
sub list_mmsc
{
    my @list = ();
    my $nr;

    foreach $nr (sort {$a <=> $b} keys %{$config{'mmsc'}} )
    {
	if (defined($config{'mmsc'}{$nr}{'id'}) && $config{'mmsc'}{$nr}{disabled} != 1) {
	    push(@list, $config{'mmsc'}{$nr}{'id'});
	}
    }

    return @list;
}

#------------------------------------------------------------------
# list_smsc_nobilling
#------------------------------------------------------------------
sub list_smsc_nobilling
{
    my @list = ();
    my @uniq = ();
    my %seen = ();
    my $nr;

    foreach $nr (sort {$a <=> $b} keys %{$config{'mmsc'}} )
    {
	if (defined($config{'mmsc'}{$nr}{'id'}) && ($config{'mmsc'}{$nr}{disabled} != 1) && 
	     (not defined($config{'mmsc'}{$nr}{'nobilling-smsc-id'}))) {
	    push(@list, $config{'mmsc'}{$nr}{'id'})
	}
    }

    @uniq = grep { ! $seen{$_} ++ } @list;
    return @uniq;
}

#------------------------------------------------------------------
# list_shortnumber
#------------------------------------------------------------------
sub list_shortnumber
{
    my @list = ();
    my @uniq = ();
    my %seen = ();
    my $nr;

    foreach $nr (keys %{$config{'mmsc'}} )
    {
	next if ($config{'mmsc'}{$nr}{disabled} == 1);

	if (defined($config{'mmsc'}{$nr}{'my-number'})) {
	    push(@list, $config{'mmsc'}{$nr}{'my-number'})
	}
	if (defined($config{'mmsc'}{$nr}{'allowed-sender'})) {
	    push(@list, split /\;/, $config{'mmsc'}{$nr}{'allowed-sender'});
	}
    }
    
    @uniq = grep { ! $seen{$_} ++ } @list;
    return @uniq;
}

#------------------------------------------------------------------
# list_tariff
#------------------------------------------------------------------
sub list_tariff
{
    my @list = ();
    my $tc;

    if (-e $config{'core'}{0}{'tariff-classes'}) {
	open(FH, "<$config{'core'}{0}{'tariff-classes'}") || ERROR("Can't open $config{'core'}{0}{'tariff-classes'}");
	while ($line = <FH>) {
	    next if ($line =~ /^#/);
	    next if ($line =~ /^smsc/);
	    next if ($line !~ /,/);

	    ($tc) = split /,/, $line;
	    push(@list, $tc);
	}
	close(FH);
    } else {
	@list = ('none');
    }

    return @list;
}

#------------------------------------------------------------------
# list_services
#------------------------------------------------------------------
sub list_services
{
    my @list = ();
    my @uniq = ();
    my %seen = ();
    my $nr;

    foreach $nr (keys %{$config{'mms-service'}}) {
	push(@list, $config{'mms-service'}{$nr}{'name'}) if (length($config{'mms-service'}{$nr}{'name'}));
    }

    foreach $nr (keys %{$config{'send-mms-user'}}) {
	push(@list, $config{'send-mms-user'}{$nr}{'name'}) if (length($config{'send-mms-user'}{$nr}{'name'}));
    }

    @uniq = grep { ! $seen{$_} ++ } @list;
    return @uniq;
}

#------------------------------------------------------------------
# login: returns immediatly, if all ready logged in,
# else do the login screen.
#------------------------------------------------------------------
sub login
{
    my $tmpl;

    # this site runs without password
    return unless(length($def{PASSWORD}));

    # password already entered and valid
    return if ($digest eq $def{PASSWORD});

    # prompt user for password
    $tmpl = open_template("login.tmpl");
    $rm = "login";
    writepage($tmpl);
}

sub add_to_list()
{
    my $nr    = shift;
    my $list  = shift;
    my $color = shift;
    my %par;

    $par{nr} = $nr;
    $par{text1} = $config{'mms-service'}{$nr}{'name'};
    $par{text1} = "--noname--" unless (defined $par{text1});
    $par{text2} = length($config{'mms-service'}{$nr}{'keyword'}) ?
			    $config{'mms-service'}{$nr}{'keyword'} :
			    $config{'mms-service'}{$nr}{'regex'};
    $par{text2} .= " (a)" if (length($config{'mms-service'}{$nr}{'aliases'}));
    $par{elink} = "$pname&rm=list&action=edit&type=mms-service&nr=$nr";
    $par{dlink} = "$pname&rm=list&action=delete&type=mms-service&nr=$nr";
    $par{clink} = "$pname&rm=list&action=copy&type=mms-service&nr=$nr";
    $par{tlink} = "$pname&rm=list&action=toggle&type=mms-service&nr=$nr";

    my $msg = $config{'mms-service'}{$nr}{'get-url'};
    $msg = $config{'mms-service'}{$nr}{'post-url'} unless(length($msg));
    $msg = $config{'mms-service'}{$nr}{'text'} unless(length($msg));
    $msg = $config{'mms-service'}{$nr}{'file'} unless(length($msg));
    $msg = $config{'mms-service'}{$nr}{'exec'} unless(length($msg));
    $msg = substr($msg,0,40).".." if (length($msg) > 41);
    $par{text3} = $msg;
    $par{color} = $$color; $$color=($$color ? 0: 1);
    $par{disabled} = $config{'mms-service'}{$nr}{disabled};

    push(@$list, \%par);
}

do_request();
1;

