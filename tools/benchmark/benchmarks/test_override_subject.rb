require 'mmslib'
require 'mbuni_benchmarking'

include MMSLIB
include DSL
include MbuniBenchmarking
include AppSimulator

class TestOverrideSubject < BenchmarkSuite

  include MbuniBenchmarking::MMSBoxModule
  include MbuniBenchmarking::TestModule
  
  def self.subject_callback(subject)
    @@subject = subject
  end
  
  def setup
    puts "running setup"
    @@subject = nil
    @app_server = MMSAppServer.new(mmsbox_setting("mms-service", "post-url"))
    @app_server.servlet = MMSAppServlet
    @app_server.asynchronous = true
    @services[:app_server] = @app_server
    super
  end
  
  def benchmark_test_subject_override_main
    fabricate_incoming_mms("mainkw blah blah foo bar di di dum")
    
    sleep 4 # wait for message to by dispatched by mbuni
    req_time, req = @app_server.requests.first
    
    assertion "We got request from mbuni" do
      not req.nil?
    end
    
    assertion "subject overridden by main keyword when main keyword used" do
      req['x-mbuni-subject'] == mmsbox_setting("mms-service", "override-subject")
    end
  end
  
  def benchmark_test_subject_override_alias
    fabricate_incoming_mms("aliaskw bum didi didi dum")

    sleep 4
    req_time, req = @app_server.requests.first
    
    assertion "We got request from mbuni" do
      not req.nil?
    end
    
    assertion "subject overridden by main keyword when main keyword used" do
      req['x-mbuni-subject'] == mmsbox_setting("mms-service", "override-subject")
    end
  end
  
  def fabricate_incoming_mms(mms_text)
    sendmms do
      mms {
        from "sender@from.com"
        to "reciepient@to.com"
        direction "MO"
        contents {
          text "#{mms_text}"
        }
      }
      via {
        url "http://localhost:" + mmsbox_setting("mbuni", "sendmms-port") + "/"
        username mmsbox_setting("send-mms-user", "username")
        password mmsbox_setting("send-mms-user", "password")
      }
    end
  end
end