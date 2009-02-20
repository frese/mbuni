require 'test/unit'

require 'mmslib'
require File.dirname(__FILE__)  + "/../lib/mbuni_benchmarking.rb"

include MbuniBenchmarking

class TestReport < Test::Unit::TestCase
	def setup
	  @b = Benchmark.new
	  @b.results = { :benchmark_test1 => true, :benchmark_test2 => true }
	  @b.start_times[:benchmark_test1] = Time.now
	  @b.start_times[:benchmark_test2] = Time.now	+ 10
	  @b.end_times[:benchmark_test1] = Time.now + 10
	  @b.end_times[:benchmark_test2] = Time.now	+ 20
	  
	  @b.start_time = Time.now
	  @b.end_time = Time.now + 30
  end
  
  def test_create_report
    @r = Report.new(@b)
	  
	  1.upto(2) do |j|
      data = {}
	    data[:sequence1] = [42]
	    data[:sequence2] = [42]
	  
	    1.upto(99) do |i|
	      data[:sequence1] << data[:sequence1].last + (rand(10)-5)
	      data[:sequence2] << data[:sequence2].last + (rand(10)-5)
      end
      graph = MbuniBenchmarking::Graph.new("Report graph #{j}", data)
      @r[:benchmark_test1].add_graph(graph)
    end
    
    @r[:benchmark_test1].add_details "This is the details for benchmark test1"
    @r[:benchmark_test2].add_details "This is the details for benchmark test2"
  end
  
  def test_write_report
    test_create_report
    @r.write
  end
  
  def teardown
    #File.unlink(@target_file) if File.exist?(@target_file)
  end
end