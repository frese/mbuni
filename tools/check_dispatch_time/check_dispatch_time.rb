#!/usr/local/bin/ruby
# This script looks through the access log to the find out how much time
# is used from receive to dispatch. Additionally it will detect when the 
# message have not been dispatch or if it has been dispatched multiple times.
# It expects one argument which is the path to the mbuni access log to examine.
# Designed also to work as a Nagios plugin (compatible return codes).
require 'date'

# @DISPATH_TIME_THRESHOLD@ is the maximal processing time allowed between a receive and a dispatch.
# Specified as an integer number of seconds.
DISPATCH_TIME_THRESHOLD=100

# Maximal number of attempts to dispatch a message. Multiple attempts will occur of the target service
# cannot be reached. It should correspond to the mbuni coniguration setting for this.
DISPATCH_ATTEMPT_THRESHOLD=3

OK=0
WARNING=1
CRITICAL=2

exitcode=OK

alog = ARGV.first
alog ||= "access.test" # just for testing
from_ordered = {}

# Return difference between two datetimes
def difference(dt1,dt2)
  f = (dt1-dt2)
  (f*f.denominator).abs
end

class MsgDispatchCheck
  def initialize(msgid, timestamps)
    @msgid, @timestamps = msgid, timestamps
  end


  def check
    if @timestamps.size == 2
      @receive_time, @dispatch_time = @timestamps
      time_to_dispatch = difference(@receive_time, @dispatch_time)
      if  time_to_dispatch > DISPATCH_TIME_THRESHOLD
        error "dispatched #{time_to_dispatch} seconds after it was received (#{DISPATCH_TIME_THRESHOLD} seconds is the maximal allowed time)"
      else
        [ OK, nil ]
      end
    elsif @timestamps.size == 1
      @receive_time = @timestamps.first
      if difference(@receive_time, DateTime.now) > DISPATCH_TIME_THRESHOLD
        error "never dispatched."
      else
        warning "never dispatched."        
      end
    elsif @timestamps.size > 2
      receive_time = @timestamps.shift
      if @timestamps.size > DISPATCH_ATTEMPT_THRESHOLD
        error "dispatched more than once at #{(@timestamps.map{|t| t.to_s}).join(', ')}"
      else
        warning "dispatched more than once at #{(@timestamps.map{|t| t.to_s}).join(', ')} (within allowed number of retries)"        
      end
    end
  end
  
  def warning(msg)
    [ WARNING, "WARNING: msgid=#{@msgid} received #{@receive_time} was #{msg}" ]
  end
  
  def error(msg)
    [ CRITICAL, "ERROR: msgid=#{@msgid} received #{@receive_time} was #{msg}" ]
  end
end

msgtrack = {}

File.open(alog).each do |line|
  if line =~ /Received MMS/
    msgtrack[$2] = [ DateTime.parse($1) ] if line =~ /^(\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d).*\[msgid:\+?(\w+)\](.*)$/
  elsif line =~ /Dispatch MMS/
    msgtrack[$2] << DateTime.parse($1) if line =~ /^(\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d).*\[msgid:\+?(\w+)\](.*)$/
  end
end

msgtrack.each do |msgid, timestamps| 
  status, reason = MsgDispatchCheck.new(msgid,timestamps).check
  if status != OK
    exitcode = status if status > exitcode
    puts reason
  end
end

exit exitcode
