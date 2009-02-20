require 'test/unit'
require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB
include DSL
include SOAP

class MIMETest < Test::Unit::TestCase
  def setup
    @gateway = gateway { url "http://localhost:42/" }
  end
  
  def test_deliver_req
     msg = mms {
       to "recipient@whatever.com"
       from "sender@whatever.com"
       contents {
         text "sample text"
       }
     }
     dl_req = MMSDeliverRequest.new(msg,@gateway)
     puts  "---------"*10
     puts dl_req.to_s
     puts "--- DONE ---"
  end
  
  def teardown
  end
end 