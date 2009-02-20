require 'simulator'

module MMSLIB
  module MMSCSimulator
    include MMSLIB::Simulator
    
    ###############################################################
    # Simulator servers
    ###############################################################

    class MMSCSimulator < AbstractSimulatorServer
      attr_reader :requests
      
      def initialize(mmsc_url, mmsc_type)
        super(mmsc_url)

        if (mmsc_type.to_s == "eaif" || mmsc_type.to_s == "nokia")
          @servlet = EAIFSimulatorServlet
        elsif (mmsc_type.to_s == "soap")
          @servlet = SoapSimulatorServlet
        end
      end
    end
    
    ###############################################################
    # Servlets
    ###############################################################
    
    class AbstractSimulatorServlet < HTTPServlet::AbstractServlet
      attr_accessor :server
      
      def initialize(http_server,simulator_server)
        super(http_server)
        @simulator_server = simulator_server
        puts "simulator_server: #{simulator_server}"
      end
      
      def self.transaction_id(key)
        @@transaction_id = key.to_s
      end

      def do_GET(req,res)
        @simulator_server.log_request(Time.now, req[@@transaction_id])

        req, res = handle_request(req,res)

        res.status = 200
        res.body = "OK\n";
      end

      alias :do_POST :do_GET
    end
    
    class EAIFSimulatorServlet < AbstractSimulatorServlet
      transaction_id "X-Mms-Transaction-ID"
      
      def initialize(http_server, simulator_server)
        super(http_server, simulator_server)
        @message_id = 0
      end

      def handle_request(req,res)
        @message_id += 1
        res['Message-ID'] = @message_id
        [req,res]
      end
    end

    class SoapSimulatorServlet < AbstractSimulatorServlet
      transaction_id "message-id"

      def initialize(self_url, gateway)
        super(self_url)
        @gateway = gateway
      end

      def send_message(to, from, message)
        MMSDeliverRequest.new(message,to,from,@gateway).send
      end

      def handle_request
        @message_id += 1;
        if req =~ /<\/mm7:TransactionID.*>(.*)<\/mm7:TransactionID>/
          transaction_id = trim($1)
        else
          transaction_id = nil
        end
        res.body = SoapResponse.success(@message_id,transaction_id).to_s
        [req,res]
      end
    end
  end
end
