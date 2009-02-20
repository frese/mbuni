#!/usr/bin/perl
#-----------------------------------------------------------------------------------
# CGI script for testing kannel
#
# This script should be registered in kannel.conf with some "keyword"
# The parameters should be : to=%p&keyword=%k&text=%r&smsc=%i&lac=%P
#
# The dlr-url registered for the keyword should be:
#    dlr-url = http://host/path/dlr.pl?smsc=%i&dlr_id=%I&msisdn=%P&dlr=%d
#
# Syntax :
#	<keyword> test		: returns a test message about smsc etc
#	<keyword> duck		: returns a test message using the duckling interface
#	<keyword> ltext		: large text
#	<keyword> logo		: returns an operator logo
#	<keyword> blank		: returns an blank operator logo
#	<keyword> group		: returns an group logo
#	<keyword> tone		: returns a ring-tone
#	<keyword> picture	: returns a picture message
#	<keyword> tariff #	: returns text message in tariff class #
#	<keyword> rec		: returns nothing, just receives the SMS
#
# Allan Frese, Realtime A/S, Oktober-2003
#-----------------------------------------------------------------------------------

use CGI qw(:standard);

my $keyword = defined(param('keyword')) ? param('keyword') : "kannel";
my $text    = defined(param('text'))    ? param('text')    : "";
my $to      = defined(param('to'))      ? param('to')      : "";
my $lac     = defined(param('lac'))     ? param('lac')     : "123";
my $smsc    = defined(param('smsc'))    ? param('smsc')    : "tdc";

# -------------------------------------------------------------------------
if ($text =~ /test/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>text</message_type>\n".
          "<text>Test (æøå).\nTo ".$to." from ".$lac." (".$smsc.")</text>\n".
	  "<dlr_mask>3</dlr_mask>".
	  "<dlr_id>1234</dlr_id>".
          "<tariff_class>0</tariff_class>\n".
	  "<validity>10</validity>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /duck/) {

    print header('text/xml');
    print "<sms_pull_response>\n".
	  "<sms_plain tariff=\"100\">\n".
	  "<text>Test (æøå).\nTo ".$to." from ".$lac." (".$smsc.")</text>\n".
	  "</sms_plain>\n".
          "</sms_pull_response>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /tom/) {

    print header('text/xml');
#    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
#    print "<sms_pull_response>\n".
#	  "<sms_plain tariff=\"0\">\n".
#	  "<text></text>\n".
#	  "</sms_plain>\n".
#          "</sms_pull_response>\n";

print "<?xml version=\"1.0\" standalone=\"no\"?>\n";
print "<!DOCTYPE SMS_PULL_RESPONSE SYSTEM \"http://compton.uni2.net/realtime/dtd/realsms.dtd\">\n";

print "<SMS_PULL_RESPONSE>\n";
  print "<STATUS CODE=\"00\" TEXT=\"OK\"/>\n";
  print "<SMS_PLAIN TARIFF=\"0\">\n";
    print "<TEXT></TEXT>\n";
    print "<META NAME=\"service-id\" CONTENT=\"52594\"/>\n";
    print "<META NAME=\"billing-id\" CONTENT=\"43710906\"/>\n";
  print "</SMS_PLAIN>\n";
print "</SMS_PULL_RESPONSE>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /ltext/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>text</message_type>\n".
          "<text>Denne text er så stor at vi må dele den i flere sms'er. ".
	  "Dvs at der er mere end 160 tegn, som er maximum for en sms. ".
	  "Den vil bliver sendt som to sms'er, som telefonen burde samle ".
	  "til èn sms.</text>\n".
          "<tariff_class>0</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /logo/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>nokia_logo</message_type>\n";
    print "<text>";

    if ($smsc =~ /orange/) {
	print "32F80300480E01";
    } elsif ($smsc =~ /tdc/) {
	print "32F81000480E01";
    } elsif ($smsc =~ /telia/) {
	print "32F80200480E01";
    } elsif ($smsc =~ /sonofon/) {
	print "32F82000480E01";
    }

    print "0000000000000000000000000000000000001FFBFF701C000003C01FFBFF701C000002".
          "1000FBC0703C03C3CFFC01E3C4703C040C221003C3DCF03C041014100F83D8F03C0310".
	  "14201F0380F03C009014201FFBFEFFBFE09024201FFBFEFFBFEF8FC41C000000000000".
	  "000000000000000000000000000000000000000000".
	  "</text>\n";

    print "<tariff_class>0</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /blank/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>nokia_logo</message_type>\n";
    print "<text>";

    if ($smsc =~ /orange/) {
	print "32F80300480E01";
    } elsif ($smsc =~ /tdc/) {
	print "32F81000480E01";
    } elsif ($smsc =~ /telia/) {
	print "32F80200480E01";
    } elsif ($smsc =~ /sonofon/) {
	print "32F82000480E01";
    }

    print "0000000000000000000000000000000000000000000000000000000000000000000000".
          "0000000000000000000000000000000000000000000000000000000000000000000000".
	  "0000000000000000000000000000000000000000000000000000000000000000000000".
	  "000000000000000000000000000000000000000000".
	  "</text>\n";

    print "<tariff_class>0</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /group/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>nokia_group</message_type>\n".
	  "<text>00480E0100000000000000000003FFFFFFFFFFFFFE000C60030000000003D01014A".
	  "08000000000F41012148000000000B8200880400000000068200841400000000074200891".
	  "400000000054200AA44000000000781010028000000000AC1011488000000000B00C60030".
	  "000000003F003FFFFFFFFFFFFFE00000000000000000000</text>\n".
          "<tariff_class>1</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /tone/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>nokia_tone</message_type>\n".
	  "<text>024A3A5D55B9ADB9BDDDB80400EB1CD511890A13890A14691610691890A1".
	  "4890A18694610691890A13890A14616813810811810824DB480944624284E24285".
	  "1A45841A46242852242861A51841A4624284E24285185A04E0420460420936D203".
	  "51369169049169469B618410411890A14890A18694690611890A13890A146168".
	  "</text>\n".
          "<tariff_class>1</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /picture/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>nokia_picture_message</message_type>\n".
	  "<text>3002010000481C01080800000000008800550000000000".
	  "0110002A42000001000020005520006002800040002AC400B005".
	  "030080001110005008068100000250180BF80A82000200202404".
	  "04128000440000260923E440008800004D09220640011000004D".
	  "F1206660022000004C1001F6700440000025E405F47808900008".
	  "67F405FEDC11200411EFF209F3CC22400823FFF1F1CF4C448010".
	  "46F3F807BC34890020869D5FFFE60C120041068FF7F826182400".
	  "80228930982230400000437130981CE08000009181308801C100".
	  "080121E0E0700F02002202407C00007C040A5004801FE01FF008".
	  "10A4090003FFFF801025501200001FF000002AA4200000000000".
	  "00554A</text>\n".
          "<tariff_class>1</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /tariff/) {

    my $tf = 0;

    ($dummy, $tf) = split(/ /, $text);

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>text</message_type>\n".
          "<text>Denne tekst er sendt i tariffclass $tf</text>\n".
          "<tariff_class>$tf</tariff_class>\n".
	  "<dlr_mask>3</dlr_mask>".
	  "<dlr_id>2345</dlr_id>".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /ems/) {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    print "<sms>\n".
          "<message_type>ems</message_type>\n".
	  "<udh>83128100090E00000003F0000000000000000CC80000000".
	  "00000001AC8000000000000003AD00180000000000058CFFE8000".
	  "00000000DECCCC800000000000DCCCCC80000000000075CCCD000".
	  "00000000005FCFD000810800880028C8D1002010880020AA4A602".
	  "2020001008018784000000400080804444102222008000003838820000".
	  "</udh>\n".
#	  "<text>Dav</text>".
          "<tariff_class>0</tariff_class>\n".
          "</sms>\n";

# -------------------------------------------------------------------------
} elsif ($text =~ /rec/) {
    print "\n";
# -------------------------------------------------------------------------
} else {

    print header('text/sms-xml-message');
    print "<?xml version=\"1.0\"?>\n";
    print "<sms>\n".
          "<message_type>text</message_type>\n".
          "<text>Usage: $keyword test, duck, ltext, logo, group, tone, picture, tariff #</text>\n".
          "<tariff_class>0</tariff_class>\n".
          "</sms>\n";
}

sub urlescape {
  my $str = shift;
  $str =~ s/([^\w_.-])/uc sprintf("%%%02x",ord($1))/eg;
  return $str;
}

