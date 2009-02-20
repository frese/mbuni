module MbuniBenchmarking
  class Graph
    attr_accessor :xlabel, :ylabel, :target_file, :name, :y_axis_key
    
    DATA_FILE=File.expand_path(TMPDIR+"/gnuplot.data")
    GNUPLOT_FILE=File.expand_path(TMPDIR+"/gnuplot.script")
    
    def initialize(name, plotdata, target_file="graph.png")
      @xlabel, @ylabel = "X", "Y"
      @name, @plotdata, @target_file = name,plotdata,target_file
      @y_axis_key = nil
    end
    
    def plot
      write_data_file
      write_gnuplot_script
      `gnuplot #{GNUPLOT_FILE}`
    end
    
    def write_data_file
      data_length = @plotdata[@plotdata.keys.pop].length
      
      File.open(DATA_FILE, "w") do |f|
        1.upto(data_length) do |i|
          if @y_axis_key.nil?
            line = [i]
          else
            line = [@plotdata[@y_axis_key][i-1]]
          end
          
          @plotdata.keys.each do |key|
            next if not @y_axis_key.nil? and key == @y_axis_key
            line << @plotdata[key][i-1]
          end
          f.write line.join("\t") + "\n"
        end
      end
    end
    
    def write_gnuplot_script
      gnuplot = []
      gnuplot << "set autoscale"
      gnuplot << "unset log"
      gnuplot << "unset label"
      gnuplot << "set xtic auto"
      gnuplot << "set title \"#{@name}\""
      gnuplot << "set xlabel \"#{@xlabel}\""
      gnuplot << "set ylabel \"#{@ylabel}\""
      gnuplot << "set term png"
      gnuplot << "set output \"#{File.expand_path(@target_file)}\""
      plots = []
      i = 1
      @plotdata.keys.each do |key|
        next if not @y_axis_key.nil? and key == @y_axis_key
        plots << "\"#{DATA_FILE}\" using 1:#{i+1} with linespoints title \"#{key.to_s}\""
        i += 1
      end
      gnuplot << "plot " + plots.join(",")
      
      File.open(GNUPLOT_FILE, "w") do |f|
        f.write(gnuplot.join("\n"))
      end
    end
  end
end