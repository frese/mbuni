module MbuniBenchmarking
  class ReportSection
    def initialize(name, report_dir, benchmark)
      @name, @report_dir, @benchmark = name.to_s, report_dir, benchmark
      @graphs = []
      @details = ""
    end
    
    def add_graph(graph)
      graph.target_file = @report_dir + "/" + @name + "-" + graph.name + ".png"
      @graphs << graph
    end
    
    def add_details(text)
      @details += text
    end
    
    def create_html
      puts @benchmark.start_times.inspect
      puts @benchmark.end_times.inspect
      puts @name.inspect
      
      html = 
        "<table width=\"100%\">" +
        "<th colspan='2' align='left'>Benchmark: #{@name}</th><tr/>" +
        "<td align='left'>Time started:</td><td align='right'>#{@benchmark.start_times[@name]}</td><tr/>\n" + 
        "<td align='left'>Time finished:</td><td align='right'>#{@benchmark.end_times[@name]}</td><tr/>\n" +
        "<td align='left'>Elapsed time:</td><td align='right'>#{@benchmark.end_times[@name] - @benchmark.start_times[@name]}</td><tr/>\n" +
        "<th colspan='2' align='left>Benchmark details</th><tr/>" +
        "<td colspan='2'>#{@details}</td>" +
        "</table>"
      
      @graphs.each do |graph|
        graph.plot
        html += "<div id='center'>\n"
        html += "<p>#{graph.name}</p>\n"
        html += "<img src='#{File.basename(graph.target_file)}' alt='#{graph.name}' />\n"
        html += "</div>\n"
      end
      
      html
    end
  end
  
  class Report
    attr_accessor :name, :sections, :report_dir
    
    def initialize(benchmark) 
      @benchmark = benchmark
      @sections = {}
      @report_dir = File.expand_path(BASEDIR+"/reports/#{@benchmark.class.to_s}/")
      
      flowerbox "Inspecting results: "
      puts @benchmark.results.inspect
      
      @benchmark.results.keys.each do |key|
        puts "Creating report section: #{key}"
        @sections[key.to_sym] = ReportSection.new(key, @report_dir, benchmark)
      end
    end

    def write
      puts "report.write"
      
      FileUtils.mkdir_p @report_dir

      File.open("#{report_dir}/report.html", "w") do |f|
        f.write report_header
        @sections.keys.each do |section_key|
          puts "write.section(#{section_key})"
          f.write @sections[section_key].create_html
        end
        f.write(report_footer)
      end
    end
    
    def report_header
      "<html><head><title>#{@benchmark.class.to_s}</title></head>\n" + 
      "<body>\n" +
      "<h1>Results for running benchmark suite #{@benchmark.class.to_s}</h1>\n" +
      "<hr/>\n"+
      "<table width=\"100%\">" +
      "<td align='left'>Time started:</td><td align='right'>#{@benchmark.start_time}</td><tr/>\n" + 
      "<td align='left'>Time finished:</td><td align='right'>#{@benchmark.end_time}</td><tr/>\n" +
      "<td align='left'>Elapsed time:</td><td align='right'>#{@benchmark.end_time - @benchmark.start_time}</td><tr/>\n" +
      benchmark_list + 
      "</table>"
    end
    
    def benchmark_list
      list = "<th align='left' colspan='2'>The Following benchmarks #{@benchmark.class.to_s} were run:</th><tr/>\n"
      list += "<td colspan='2'><ul>"
      @benchmark.results.keys.each do |key|
        list += "<li>#{key}</li>"
      end
      list += "</ul></td><tr/>"
    end
    
    def report_footer
      "</body></html>"
    end
    
    def [](idx)
      @sections[idx.to_sym]
    end
  end
end
