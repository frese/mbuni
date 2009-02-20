module MMSLIB
  class ProcessMonitor
    attr_accessor :monitor_interval

    OS_ASPECTS = {
      "Darwin" => {
        "vsize" => "vsize",
        "pcpu" => "pcpu"
      },
      "SunOS" => {
        "vsize" => "vsz",
        "pcpu" => "pcpu"
      }
    }
    
    def initialize(pid, monitor_aspects)  
      @pid = pid
      raise "Unsupported architechture: #{uname}" if OS_ASPECTS[uname].nil?
      @aspects = monitor_aspects.keys.map { |key| mapkey(key) }
      @monitor_interval = {}
      @reverse_key_map = {}
      monitor_aspects.keys.map do |key| 
        @monitor_interval[mapkey(key)] = monitor_aspects[key]
        @reverse_key_map[mapkey(key)] = key
      end
      @running = false
    end

    def start 
      @pid = @pid.pid unless @pid.class == Fixnum

      raise "Cannot start - process monitor is already running" if @running == true
      @threads = []
      @results = {}
      @running = true
      @aspects.each do |a|
        #puts "starting thread for aspect " + a.to_s
        @threads << Thread.new { monitor_aspect(a) }
      end
    end

    def stop
      raise "Cannot stop - process monitor is not running" if @running == false
      @running = false
      @threads.each { |t| t.join }
    end

    def monitor_aspect(aspect)
      @results[aspect] = []
      while @running
        sleep @monitor_interval[aspect]
        res = ps_poll(aspect)

        # weed out bad results
        next unless res =~ /(\d|\.)+/

        @results[aspect] << res
      end
    end

    def ps_poll(aspect)
      (`ps -o #{aspect.to_s} -p #{@pid}`).to_a.pop.trim
    end
    
    def uname
      IO.popen("uname", "r").read.chomp
    end
    
    def mapkey(key)
      OS_ASPECTS[uname][key.to_s]
    end
    
    def results
      orig_key_results = {}
      @results.map { |key,val| orig_key_results[@reverse_key_map[key]] = val }
      orig_key_results
    end
  end
end
