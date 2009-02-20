#
# A class used to process Mbuni logs
#
module MbuniBenchmarking
  class LogAnalyzer
    def initialize(logfile)
      @logfile = logfile
    end

    def analyze_errors()
      log_errors = {}

      File.open(@logfile, "r") do |f|
        f.each do |line|
          if line =~ /^.*ERROR:(.*)$/
            log_errors[$1] ||= 0 # initialize first time
            log_errors[$1] += 1
          end
        end
      end
      log_errors
    end
  end
end