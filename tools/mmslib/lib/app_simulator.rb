# We need to this because of load-order

require File.dirname(__FILE__) + '/simulator.rb'

module MMSLIB
  module AppSimulator
    include Simulator

    class MMSAppServlet < AbstractSimulatorServlet
      transaction_id "x-mbuni-transactionid"
    end

    class MMSAppServer < AbstractSimulatorServer; end
  end
end
