require 'mmslib'
require 'mbuni_benchmarking'

include MMSLIB
include DSL
include MbuniBenchmarking

class MT < BenchmarkSuite
  include MbuniBenchmarking::MMSBoxModule
  include MbuniBenchmarking::MMSCSimulatorModule
  
  ################################################################################
  # setup and teardown - these are run before and after each benchmark is run, 
  # respectively
  ################################################################################
  def setup
    @msg_send_interval = cfg('message-send-interval')
    @num_msgs = cfg('messages-to-send')
    @originator = cfg('message-from')
    @recipient = cfg('message-to')
    super
  end
  
  ################################################################################
  # The actual benchmark methods
  ################################################################################  
  
  def benchmark_small_message
    send_loop_small_msg(@num_msgs, @msg_send_interval)
  end
  
  def benchmark_big_message
    send_loop_big_msg(@num_msgs, @msg_send_interval)
  end
end