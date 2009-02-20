#!/usr/local/bin/ruby
# This script looks through the access log to find messages from the the same
# sender, which are received with very short intervals.
# Designed also to work as a Nagios plugin
# It expects one argument which is the path to the mbuni access log to examine
require 'date'

# Looks for messages received within @TIMEFRAME_THRESHOLD@ seconds from each other from the same sender:
TIMEFRAME_THRESHOLD=1

# If more than @WARNING_THRESHOLD@ messages are received with @TIMEFRAME_THRESHOLD@, generate a warning
WARNING_THRESHOLD=2
# If more than @CRITICAL_THRESHOLD@ messages are received with @TIMEFRAME_THRESHOLD@, generate an critical error
CRITICAL_THRESHOLD=3

OK=0
WARNING=1
CRITICAL=2

generate_warning=false
generate_critical=false

alog = ARGV.first
alog ||= "access.test" # just for testing
from_ordered = {}

# Return difference between two datetimes
def difference(dt1,dt2)
  f = (dt1-dt2)
  i = f*f.denominator
  i.abs
end

File.open(alog).each do |line|
	# We are only interested in Received MMS
	next unless line =~ /Received MMS/
	next unless line =~ /^(\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d).*\[from:\+?(\d+)\](.*)$/
	d = DateTime.parse($1)
	from = $2
	from_ordered[from] ||= []
	from_ordered[from] << d
end

warnings = []
from_ordered.each do |from,dates|
  sdates = dates.sort
  
  skip_to=0
  0.upto sdates.size-1 do |i|
    next if skip_to > i
    
    count = 1
    (i+1).upto sdates.size-1 do |j|
      break unless difference(sdates[i], sdates[j]) < TIMEFRAME_THRESHOLD
      count += 1
    end
    
    if count > 1
      warnings << "#{sdates[i].strftime('%Y-%m-%d %H:%M:%S')} | #{count} messages from #{from} received within #{TIMEFRAME_THRESHOLD} second(s) of #{sdates[i].strftime('%Y-%m-%d %H:%M:%S')}"
      skip_to = i + count
      generate_warning=true if count >= WARNING_THRESHOLD
      generate_critical=true if count >= CRITICAL_THRESHOLD
    end
  end
end

# output warnings: strip prefixed date which is only used for sorting
warnings.sort.each do |warning|
  puts warning.split("|").last
end

if generate_critical
  exit CRITICAL
elsif generate_warning
  exit WARNING
else
  exit OK
end