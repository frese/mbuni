require 'test/unit'

require File.dirname(__FILE__) + '/../mmslib.rb'
include MMSLIB

class TestMbuniControl < Test::Unit::TestCase
	def setup
		teardown
		Dir.mkdir("/tmp/spool")
		File.open("/tmp/test_mmsbox.conf", "w") { |f| f.write(mmsbox_conf) }

		@mc = MbuniControl.new("/opt/realtime/mbuni", "/tmp/test_mmsbox.conf")
	end

	def teardown
		[ "test_mmsbox.log", "test_access.log", "test_mmsbox.conf"].each do |f|
			File.unlink("/tmp/" + f) if File.exist?("/tmp/" + f)
		end	
		system "rm -rf /tmp/spool"
	end

	def test_with_redirection
		pid = @mc.start("mmsbox")
		sleep 10
		log_start = File.open("/tmp/test_mmsbox.log","r").read 
		status = @mc.stop("mmsbox")
		sleep 10
		log_stop = File.open("/tmp/test_mmsbox.log","r").read 

		assert(pid != nil, "pid is nil")
		assert(status != nil, "stop status code")
		assert(log_start =~ / Mbuni MMSBox .* starting/, "startup in log")
		assert(log_stop =~ /shutdown in progress/, "shutdown in log")  
  end
	
	def mmsbox_conf
conf=<<EOF
group = core
log-file = /tmp/test_mmsbox.log 
access-log = /tmp/test_access.log
log-level = 0

group = mbuni
storage-directory = /tmp/spool 
max-send-threads = 1 
maximum-send-attempts = 50
default-message-expiry = 360000
queue-run-interval = 5
send-attempt-back-off = 300
sendmms-port = 10001
admin-port = 40000
admin-password = secret
admin-allow-ip = "127.0.0.1"
admin-deny-ip = "*.*.*.*"
EOF
	end

end
