require 'mms_dsl.rb'

module MMSLIB
  class MbuniConfiguration
    attr_accessor :filename

    def initialize(filename)
      @filename = filename
      @configuration = {}
      current_group = nil
      File.open(filename, "r") do |f|
        f.each do |line|
          line = $1.reverse if line.reverse =~ /^.*#(.*)$/ # remove comments (reversing because ruby regex are greedy)
          
          next if line =~ /^\s*$/ # skip empty lines and comment lines

          if line =~ /^(.*)\s*=\s*(.*)$/
            key = $1.trim
            value = $2.trim.unquote

            if key == "group"
              @configuration[value] = [] if @configuration[value].nil?
              @configuration[value] << {}
              current_group = value
            else
              @configuration[current_group].last[key] = value
            end
          else
            raise "Cannot parse line: #{line}"
          end
        end
      end
    end

    def [](idx)
      @configuration[idx]
    end

    def self.load_global_configuration(file="/etc/mbuni.def")
      @global_configuration = file
      instance_names = []
      File.open(@global_configuration) do |conf|
        conf.each do |line|
          if line =~ /^INSTANCES=(.*)/
            instance_names = $1.split(",")
          elsif line =~ /^MBUNI_HOME=(.*)/
            @@mbuni_home = $1
          end
        end
      end
      
      @@instances = {}
      instance_names.each do |name|
        @@instances[name] = Instance.new(name, MbuniConfiguration.new("#{@@mbuni_home}/etc/mbuni.#{name}.conf"))
      end
    end

    def self.instances
      load_global_configuration unless @@instances
      @@instances.to_a.map { |i| i.last }
    end

    def self.find_instance_by_name(name)
      load_global_configuration unless @@instances
      @@instances[name]
    end
  end

  include DSL

  # Abstraction over a single Mbuni instance
  class Instance
    attr_accessor :name, :configuration
    
    
    def initialize(instance_name, configuration)
      @name = instance_name
      @configuration = configuration
    end

    def self.[](idx)
      MbuniConfiguration.find_instance_by_name(idx)
    end

    def self.find_by_domain(domain)
      domain = $1.trim if domain =~ /^.*@(.*)$/
      MbuniConfiguration.instances.each do |instance|
        next unless email_domains_str = instance.configuration["mbuni"].first["email-domains"]
        email_domains = email_domains_str.split(",").map {|i| i.trim }
        return instance if email_domains.include?(domain)
      end
      nil
    end

    def gateway_for_user(sendmms_user = nil)
      
      # first, find the sendmms-port
      mbuni_grp = @configuration["mbuni"].first
      sendmms_port = mbuni_grp["sendmms-port"]
      
      # Use the *sendmms_user* if available, otherwise just use the first available user
      usr = (@configuration["send-mms-user"].select { |u| u['username'] == sendmms_user }).first
      usr ||= @configuration["send-mms-user"].first

      gateway {
        url "http://127.0.0.1:#{sendmms_port}/"
        username usr["username"]
        password usr["password"]
      }
    end
    
    def to_s
      name
    end
  end
end