require 'test/unit'

class TestMbuniBenchmark < Test::Unit::TestCase
  include MbuniBenchmarking

  class BenchmarkSpecific < MbuniBenchmarking::Benchmark; end
  
  def setup
    File.open("/tmp/mmsbox.conf", "w") { |f| f.write(mmsbox_conf) }
    File.open("/tmp/benchmark.conf", "w") { |f| f.write(benchmark_conf) }

    @bm = BenchmarkSpecific.new("/tmp/benchmark.conf")
  end
  
  def teardown
    File.unlink("/tmp/benchmark.conf")
    File.unlink("/tmp/mmsbox.conf")
  end

  def test_cfg
    assert(nil != @bm)
    assert(@bm.cfg("mbuni-dir")=="/specific/mbuni")
    assert(@bm.cfg("mmsbox.conf")=="/tmp/mmsbox.conf")
  end
  
  def test_mmsbox_settings
    assert(nil != @bm)
    assert(@bm.mmsbox_setting("core", "log-file")=="/tmp/test_mmsbox.log", "mmsbox_setting(core,log-file)")
    assert(@bm.mmsbox_setting("mbuni", "sendmms-port")=="10001", "mmsbox_setting(mbuni,sendmms-port)")
  end
  
  def benchmark_conf
    conf=<<EOF
Benchmark:
  mbuni-dir: /other/mbuni
  mmsbox.conf: /tmp/mmsbox.conf

BenchmarkSpecific:
  mbuni-dir: /specific/mbuni
EOF
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