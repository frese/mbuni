require 'webrick'
include WEBrick
require 'yaml'

require File.dirname(__FILE__) + '/mms_save_servlet.rb'

class MMSAppServer
  def initialize(config_file) 
    puts "Loading configuration from: " + config_file
    load_config(config_file)
  end

  def load_config(config_file)
    @config = open(config_file) { |f| YAML.load(f) }
  end

  def run_server
    puts "Starting " + self.class.to_s + " on port " + @config["port"].to_s
    s = HTTPServer.new( :Port => @config["port"] ) 
    s.mount("/savemms", MMSSaveServlet, @config['attachment-directory'])

    trap("INT") do
      s.shutdown 
    end

    s.start
  end
end
