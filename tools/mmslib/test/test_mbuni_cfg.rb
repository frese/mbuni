require 'test/unit'

require File.dirname(__FILE__) + '/../mmslib.rb'
include MMSLIB


class MbuniConfigurationTest < Test::Unit::TestCase
  def setup
    @instances = [ "algeria.dz", "comcel.co", "dummy", "iam.ma", "realtime.dk", "tdc.dk" ]
    @global_mbuni_conf = "/tmp/mbuni.def"
    @mbuni_home = File.dirname(__FILE__) + "/sample_mbuni_home/"
    create_global_mbuni_cfg
  end
  
  def teardown
    delete_global_mbuni_cfg
  end
  
  def test_001_parse_mmsbox_conf
    MbuniConfiguration.load_global_configuration(@global_mbuni_conf)
    @instances.each do |i|
      assert(nil != MbuniConfiguration.find_instance_by_name(i))
    end
    assert(nil == MbuniConfiguration.find_instance_by_name("iuwherpiuhwer"))
  end
  
  def test_002_instance
    MbuniConfiguration.load_global_configuration(@global_mbuni_conf)
    @instances.each { |i| assert(nil != Instance[i]) }
    assert(Instance["not existing"] == nil)
    
    assert(Instance["dummy"] == Instance.find_by_domain("dummy.com"), "find_by_domain")
  end
  
  def test_003_multigroup
    MbuniConfiguration.load_global_configuration(@global_mbuni_conf)
    assert(Instance["dummy"].configuration["send-mms-user"].size == 4, "There should be four sendmms users")
  end
  
  def test_004_gateway
    MbuniConfiguration.load_global_configuration(@global_mbuni_conf)
    gw = Instance['dummy'].gateway_for_user("mail")
    assert(nil != gw, "gw not nil")
    assert(gw.url == "http://127.0.0.1:45454/", "gateway url")
    assert(gw.username == "mail")
    assert(gw.password == "secret")
  end
  
  def create_global_mbuni_cfg
      File.open(@global_mbuni_conf, "w") do |f|
        f << "# Mbuni/Kannel definitions file\n"
        f << "INSTANCES=#{@instances.join(',')}" + "\n"
        f << "MBUNI_HOME=#{@mbuni_home}\n"
      end
  end
  
  def delete_global_mbuni_cfg
    File.unlink(@global_mbuni_conf)
  end
end