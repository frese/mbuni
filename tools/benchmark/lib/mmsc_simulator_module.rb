module MbuniBenchmarking
  module MMSCSimulatorModule
    def mmsc_simulator_module_setup
      @services[:mmsc] = MMSCSimulator::MMSCSimulator.new(*(["mmsc-url","type"].map { |i| mmsbox_setting("mmsc", i) }))
      @services[:mmsc].asynchronous = true
    end

    def mmsc_simulator_module_teardown
      # Add the number of requests per second received by the mmsc simulator
      add_result(:mmsc_msgs_received, @services[:mmsc].requests.length)
      add_result(:mmsc_requests_per_second, @services[:mmsc].requests_by_second)
    end
    
    def mmsc_simulator_module_create_report(key)
      # Create mmsc simulator requests per second graph
      if (get_result(key, :mmsc_msgs_received).to_i > 0) 
        #puts "Report requests per second"
        #puts "Inspecting: " + get_result(key, :mmsc_requests_per_second).inspect
        sec = 0
        req_sec = get_result(key, :mmsc_requests_per_second)
        req_sec[:time].map! { sec+=1 }
        
        g = Graph.new("MMSC requests per second",req_sec)
        g.y_axis_key = :time
        g.ylabel = "Number of requests received per second"
        g.xlabel = "Time/1 second"
        @report[key].add_graph(g)
      end
    end
  end
end
