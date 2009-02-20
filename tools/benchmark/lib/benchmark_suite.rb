require 'yaml'
require 'mmslib'

module MbuniBenchmarking
  include MMSLIB
  
  class Configuration
    DEFAULT_CONFIG= BASEDIR +  "/conf/benchmarks.conf"
    @@instances = {}
    
    def self.[](configuration_file=nil)
      configuration_file ||= DEFAULT_CONFIG
      begin
        @@instances[configuration_file] ||= File.open(configuration_file) { |f| YAML::load(f) }
      rescue Exception => load_fail
        puts "CONFIGURATION FILE ERROR:"
        puts load_fail.to_s
        exit
      end
    end
  end
  
  class BenchmarkSuite
    # These are shared between all subclasses
    @cfg = nil
    @@registered_benchmarks = []
    @@module_settings = {}
    
    attr_accessor :results, :start_times, :end_times, :start_time, :end_time
    
    def initialize(cfg_file=nil)
      @cfg = cfg_file
      @start_times = {}
      @end_times = {}
      @services = {}
    end
    
    def setup_methods
      (self.methods.select { |m| m =~ /.+_setup$/ }).map { |m| m.to_sym }
    end
    
    def teardown_methods
      (self.methods.select { |m| m =~ /.+_teardown$/ }).map { |m| m.to_sym }
      
    end
    
    def create_report_methods
      (self.methods.select { |m| m =~ /.+_create_report$/ }).map { |m| m.to_sym }
    end
    
    
    def self.all_extending_classes
      ObjectSpace.each_object(Class) do |obj|
        yield obj if obj.superclass == self
      end
    end
    
    # Look for parameter in group for this class, or if not found here, in group for parent class    
    def self.cfg(parameter)
      search = self.ancestors
#      puts search.inspect
      result = nil
      while result.nil?
        cls = (search.shift).to_s.gsub(/^.*::/, "")
        break if cls=="Object" # stop search here
        Configuration[nil][cls][parameter] rescue next
        result = Configuration[nil][cls][parameter]
      end
      
      throw Exception.new("Could find not '#{parameter.to_s}' setting in configuration") if result.nil?
      
      return result
    end
    
#    def self.cfg_set(parameter)
#      puts "class level cfg_set(#{parameter})"
#      begin
#        val = cfg(parameter)
#        puts "cfg_set(#{parameter}): " + ("!"*40) + val
#      rescue Exception => e
#        puts "=> false (due to exception: #{e.to_s})"
#        return false
#      end
#      puts "=> " + (val=="true" or val=="yes" or val="on").to_s
#      return (val=="true" or val=="yes" or val="on")
#    end
    
    def self.cfg_set(parameter)
      begin
        cfg(parameter)
      rescue Exception => e
        false
      end
    end
    
    def cfg(parameter)
      self.class.cfg(parameter)
    end
    
    def cfg_set(parameter)
      puts "object level cfg_set(#{parameter})"
      self.class.cfg_set(parameter)
    end
    
    def mmsbox_setting(group, key)
      File.open(cfg("mmsbox.conf")) do |f|
        ingroup = false
        f.each do |line|
          if line =~ /group *=(.*)/
            ingroup = ($1.trim == group) ? true : false
          end
          next unless ingroup and line =~ /#{key}.*=(.*)/
          return $1.trim
        end
      end
      throw Exception.new("#{key} in #{group} not found in file #{cfg('mmsbox.conf')}")
    end
    
    def self.run_all
      all_extending_classes do |bm|
        begin
          bm.new.run          
        rescue Exception => whoops
          puts "ERROR: " + whoops.to_s
          puts whoops.backtrace
          stop_all
        end
      end
    end
    
    def self.stop_all
      @@running.teardown
    end

    def run(specific=nil)
      @results = {}
      @start_times = {}
      @end_times = {}
      
      # Setup which benchmarks to run
      if specific.nil?
        benchmark_methods = self.methods.select { |m| m =~ /^benchmark.*$/ }
      elsif specific.class == Symbol or specific.class == String # One specific test
        benchmark_methods = [ specific ]
      end
      
      flowerbox "run: #{specific.to_s}"
      puts "class: " + specific.class.to_s
      puts "benchmark methods: " + benchmark_methods.inspect
      
      @start_time = Time.now
      benchmark_methods.each do |meth|
        puts "running benchmark: #{self.class.to_s}-#{meth.to_s}"
        setup
        begin
          @@running = self
          @start_times[meth] = Time.now
          flowerbox meth.to_s
          @running_benchmark = meth.to_sym
          @results[@running_benchmark] = {}
          send(meth.to_sym)
          @end_times[meth] = Time.now
        rescue Exception => whoops
          flowerbox(whoops.to_s, "!")
          puts whoops.backtrace
          teardown
          raise whoops
        end
        teardown      
      end
      @end_time = Time.now
      create_report
    end
    
    def add_result(key, result_set)
      #puts "add_result(#{key}, #{result_set.inspect})"
      @results[@running_benchmark][key] = result_set
    end
    
    def get_result(benchmark, key)
      #puts "get_result(#{benchmark}, #{key}) = #{@results[benchmark][key].inspect}"
      @results[benchmark][key]
    end
    
    def start_services
      @services.values.each { |s| s.start }
    end
    
    def stop_services
      @services.values.each do |s|
        puts "Attempting to stop service: " + s.to_s
        s.stop
      end
    end

    # There are just stub methods. Concrete benchmarks should override these.
    def setup
      setup_methods.each { |m| self.send(m) }
      start_services
      
      wait_time = cfg('wait-after-startup')
      if not wait_time.nil?
        1.upto(wait_time) do |i|
          puts "Waiting #{wait_time-i} seconds before commencing benchmarks"
          sleep 1
        end
      end
    end
    
    def teardown
      wait_time = cfg('wait-before-shutdown')
      if not wait_time.nil?
        1.upto(wait_time) do |i|
          puts "Waiting #{wait_time-i} seconds before shutting down services"
          sleep 1
        end
      end
      
      stop_services
      
      teardown_methods.each { |m| self.send(m) }
    end
    
    def create_report
      @report = Report.new(self)
      @results.keys.each do |key|
        begin
          create_report_methods.each { |m| self.send(m, key) }
        rescue Exception => oops
          puts oops.to_s
          puts oops.backtrace
        end
      end
      @report.write
    end
    
    # DSL like functionality for report creation
    def report
      @report = Report.new(self)
      @results.keys.each do |key|
        yield key
      end
      @report.write
    end
  end
end
