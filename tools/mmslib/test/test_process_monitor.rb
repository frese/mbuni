require 'test/unit'

require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB

class PIDHolder
  attr_accessor :pid
end

class TestProcessMonitor < Test::Unit::TestCase
	def test_gather_data
	  pid = fork do
	    # Runs for quite some time and comsume some memory
	    exec "ruby -e \"a = ['test']; 1.upto(100000000) do a << a; end\""
    end
    
	  p = ProcessMonitor.new(pid, { :pcpu => 0.5, :vsize => 0.5 })
	  p.start
	  puts "Started process monitor"
	  Process.waitpid(pid)
	  p.stop
	  assert(p.results)
	  assert(p.results[:pcpu].length > 0, "some cpu data gathered")
	  assert(p.results[:vsize].length > 0, "some cpu data gathered")
	  p.results[:pcpu].each do |res|
	    assert(res =~ /(\d|\.)*/, "all gathered data is numeric")
    end
    
    p.results[:vsize].each do |res|
	    assert(res =~ /(\d|\.)*/, "all gathered data is numeric")
    end
  end

  def test_with_proc
    pid_holder = PIDHolder.new
    
	  p = ProcessMonitor.new(pid_holder, { :pcpu => 0.5, :vsize => 0.5 })
    
	  pid_holder.pid = fork do
	    # Runs for quite some time and comsume some memory
	    exec "ruby -e \"a = ['test']; 1.upto(100000000) do a << a; end\""
    end
	  
	  p.start
	  puts "Started process monitor"
	  Process.waitpid(pid_holder.pid)
	  p.stop
	  assert(p.results)
	  assert(p.results[:pcpu].length > 0, "some cpu data gathered")
	  assert(p.results[:vsize].length > 0, "some cpu data gathered")
	  p.results[:pcpu].each do |res|
	    assert(res =~ /(\d|\.)*/, "all gathered data is numeric")
    end
    
    p.results[:vsize].each do |res|
	    assert(res =~ /(\d|\.)*/, "all gathered data is numeric")
    end
  end
end
