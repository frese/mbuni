require 'test/unit'

require 'mmslib'
require File.dirname(__FILE__)  + "/../lib/mbuni_benchmarking.rb"

# Quite simplistic testcase, but it is difficult to test what it 
# generates is correct
class TestGraph < Test::Unit::TestCase
	def setup
    data = {}
	  data[:sequence1] = [42]
	  data[:sequence2] = [42]
	  
	  1.upto(99) do |i|
	    data[:sequence1] << data[:sequence1].last + (rand(10)-5)
	    data[:sequence2] << data[:sequence2].last + (rand(10)-5)
    end
    
    @target_file = "tmp/testgraph.png"
    @graph = MbuniBenchmarking::Graph.new("My test graph", data, @target_file)
    
  end
  
  def test_plot
    @graph.plot
    assert(File.exist?(@target_file), "Test that a graph was generated")
  end
  
  def teardown
    File.unlink(@target_file) if File.exist?(@target_file)
  end
end