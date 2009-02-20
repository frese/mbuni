module MMSLIB
  module Simulator
    require 'webrick'    
    include WEBrick

    class AbstractSimulatorServlet < HTTPServlet::AbstractServlet
      def self.transaction_id(key)
        @@transaction_id = key.to_s
      end

      def initialize(http_server, simulator_server)
	      super(http_server)
	      @simulator_server = simulator_server
      end

      # do nothing, override if needed
      def handle_request(req,res)
        [req,res]
      end

      def do_GET(req,res)
        @simulator_server.log_request(Time.now, req)

        req, res = handle_request(req,res)

        res.status = 200
        res.body = "OK\n";
      end

      alias :do_POST :do_GET
    end
    
    class AbstractSimulatorServer
      attr_reader :requests
      attr_accessor :asynchronous, :servlet, :log_requests
      include WEBrick

      def initialize(url)
        @url = URI.parse(url)
        @requests = []
        @asynchonous = false
        @log_requests = true
      end

      def log_request(time, req)
        puts "log_request(#{time},#{req})"
        @requests << [time, req]
      end
      
      def requests_by_second
        by_second = {}

        @requests.each do |request_time, req|
          if by_second[request_time.to_i].nil?
            by_second[request_time.to_i] = 1
          else
            by_second[request_time.to_i] += 1
          end
        end
        result_set = {}
        result_set[:time] = []
        result_set[:requests] = []
        
        by_second.keys.sort.each do |k|
          result_set[:time] << k
          result_set[:requests] << by_second[k]
        end
        
        result_set
      end

      def start
        puts "Starting " + self.class.to_s + " on port " + @url.port.to_s
        raise "@servlet is nil" if @servlet.nil?
        @server = HTTPServer.new( :Port => @url.port )
        @server.mount(@url.path, @servlet, self)
        
        puts "servlet mounted at " + @url.path.to_s
        
        if asynchronous
          @server_thread = Thread.new { real_start }
        else
          real_start
        end
      end
      
      def real_start
        trap("INT") { @server.shutdown }
        @server.start
      end

      def stop
        @server.shutdown
        @server_thread.join unless @server_thread.nil?
      end
    end
  end
end
