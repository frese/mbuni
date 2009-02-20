require 'test/unit'
require 'net/http'
require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB

class MMSCSimulatorTest < Test::Unit::TestCase
  def setup
    @mmsc_url = "http://localhost:73733/test"
    @mmsc = MMSLIB::MMSCSimulator::MMSCSimulator.new(@mmsc_url, "eaif")
    @mmsc.asynchronous = true
    @mmsc.start
    sleep 1
  end
  
  def test_requests_log
    requests_to_send = 2
    
    1.upto(requests_to_send) do |i|
      uri = URI.parse(@mmsc_url)
      http = Net::HTTP.new(uri.host, uri.port)
      response,body = http.post(uri.path, "Just some test message. Not even valid eaif or anything.", { "X-Mbuni-Transaction" => i.to_s })
    end
    
    assert((not @mmsc.requests.nil?), "mmsc.requests is not nil")
    assert(@mmsc.requests.length == requests_to_send, "number of logged received requests matches number sent.")
  end
  
  def test_requests_by_second
    requests_to_send = 2
    
    1.upto(requests_to_send) do |i|
      uri = URI.parse(@mmsc_url)
      http = Net::HTTP.new(uri.host, uri.port)
      response,body = http.post(uri.path, "Just some test message. Not even valid eaif or anything.", { "X-Mbuni-Transaction" => i.to_s })
      sleep 1
    end
    
    assert(@mmsc.requests_by_second[:requests].length == 2, "An entry for each second in requests_by_second[:time]")
    assert(@mmsc.requests_by_second[:requests].length == 2, "An entry for each second in requests_by_second[:requests]")    
  end
  
  def teardown
    @mmsc.stop
    @mmsc = nil
  end
end
