# This module is used as mixin for benchmark suites that require an mmsbox running
# It provices process control, monitoring, log analyzing and reporting
module MbuniBenchmarking
  module MMSBoxModule
    ####################################################################################
    # Module setup and teardown
    def mmsbox_module_setup
      return if cfg_set("mmsbox-module-disable-setup")
      
      unlink_log_files

      # Make sure store dir exists
      store_dir = mmsbox_setting("mbuni", "storage-directory")
      FileUtils.mkdir_p(store_dir) unless File.exist?(store_dir)

      # Setup services
      @services[:mmsbox] = MbuniControl.new(cfg('mbuni-dir'), cfg('mmsbox.conf'))
      if cfg_set("mmsbox-monitor-process")
        @services[:mmsbox_process_monitor] = ProcessMonitor.new(@services[:mmsbox], {:pcpu => 0.5, :vsize => 0.5 })
      end
    end

    # Module teardown
    def mmsbox_module_teardown
      return if cfg_set("mmsbox-module-disable-teardown")
      
      if cfg_set("mmsbox-monitor-process")
        # Add process monitor results
        add_result(:mmsbox_cpu_usage, @services[:mmsbox_process_monitor].results[:pcpu])
        add_result(:mmsbox_memory_usage, @services[:mmsbox_process_monitor].results[:vsize])
      end

      # Analyze log-files for errors and add result
      if cfg_set("mmsbox-analyze-logs")
        l = LogAnalyzer.new(mmsbox_setting("core", "log-file"))
        add_result(:mmsbox_log_errors, l.analyze_errors)
      end

      unlink_log_files
      remove_store
    end

    ####################################################################################
    # Module report creation
    def mmsbox_module_create_report(key)
      return if cfg_set("mmsbox-module-disable-report")
      
      @report[key].add_details(msgs_overview_html(key)) if cfg_set("mmsbox-message-overview")

      if cfg_set("mmsbox-monitor-process")
        # Create cpu usage graph
        g = Graph.new("CPU Usage", { :mmsbox_cpu_usage => get_result(key, :mmsbox_cpu_usage) })
        g.ylabel = "Percentage CPU"
        g.xlabel = "Time/0.5 secs"
        @report[key].add_graph(g)

        # Create memory usage graph
        g = Graph.new("Memory usage", { :mmsbox_memory_usage => get_result(key, :mmsbox_memory_usage) })
        g.ylabel = "Virtual memory consumed"
        g.xlabel = "Time/0.5 secs"
        @report[key].add_graph(g)
      end

      @report[key].add_details(log_errors_html(key)) if cfg_set("mmsbox-analyze-logs")
    end

    # Reporting methods
    def msgs_overview_html(benchmark)
      html = "<table border='1'>\n"
      html += "<th colspan=2>Benchmark details</th>\n"
      html += "<tr><td>Messages sent</td><td>" + get_result(benchmark, :msgs_sent).to_s + "</td></tr>\n"

      # This is really from the mmsc simulator..
      if not get_result(benchmark, :mmsc_msgs_received).nil?
        html += "<tr><td>Messages received by MMSC</td><td>" + get_result(benchmark, :mmsc_msgs_received).to_s + "</tr></td>\n"
      end

      html += "</table>"
    end

    # Create a report section on the errors in the mmsbox.log
    def log_errors_html(benchmark)
      log_errors = get_result(benchmark, :mmsbox_log_errors)
      return "" if log_errors.keys.length == 0

      html = "<table border='1'><th>Log errors</th><th>Count</th>\n"
      log_errors.keys.each do |key|
        html += "<tr><td>#{key}</td><td>" + log_errors[key].to_s + "</td></tr>\n"
      end
      html += "</table>"
    end

    ####################################################################################    
    # Message sending stuff
    def send_loop_big_msg(msgs,delay)
      1.upto(msgs) do
        sendmms do 
          mms {
            to @recipient
            from @originator
            contents {
              image BASEDIR+"/data/big_image.jpg"
              text "A long repeated text about nothing. "
            }
          }

          via {
            url "http://localhost:" + mmsbox_setting("mbuni", "sendmms-port") + "/"
            username mmsbox_setting("send-mms-user", "username")
            password mmsbox_setting("send-mms-user", "password")          
          }
        end
      end
      add_result(:msgs_sent, msgs)    
    end

    def send_loop_small_msg(msgs,delay)
      1.upto(msgs) do
        sendmms do
          mms {
            to @recipient
            from @originator
          }
          via {
            url "http://localhost:" + mmsbox_setting("mbuni", "sendmms-port") + "/"
            username mmsbox_setting("send-mms-user", "username")
            password mmsbox_setting("send-mms-user", "password")
          }
        end
      end
      sleep delay
      add_result(:msgs_sent, msgs)
    end

    ####################################################################################  
    # Clean up methods
    def unlink_log_files
      # Make sure mbuni directories exist
      [ "log-file", "access-log" ].each do |log|
        logfile = mmsbox_setting("core", log)
        File.unlink(logfile) if File.exist?(logfile)
        FileUtils.mkdir_p File.dirname(logfile) unless File.exist?(File.dirname(logfile))
      end
    end

    def remove_store
      store_dir = mmsbox_setting("mbuni", "storage-directory")
      puts "Removing store dir"
      `rm -rf #{store_dir}` if File.exist?(store_dir)
    end
  end
end