module MMSLIB
  class MbuniControl
    class MbuniProc
      attr_accessor :pid, :exitstatus

      def initialize(stdin,stdout,stderr,pid)
        @stdin, @stdout, @stderr, @pid = stdin,stdout,stderr,pid
      end
    end

    def initialize(path, config)
      @path, @config = path, config
      @proc = {}
    end
    
    def pid(boxname="mmsbox")
      @proc[boxname].pid
    end

    # Writes a lot of crap to stdout and stderr
    def start(boxname="mmsbox", method="simple_fork")
      raise "Box with #{boxname} is already running" unless @proc[boxname].nil?
      if method == "simple_fork"
        @proc[boxname] = MbuniProc.new(nil,nil,nil,
          fork { exec "#{@path}/bin/#{boxname} #{@config}" }
        )
      elsif method == "custom_popen"
        @proc[boxname] = MbuniProc.new(*custom_popen("#{@path}/bin/#{boxname} #{@config}"))
      end

      puts "Started #{boxname} with pid=#{@proc[boxname].pid} using method #{method}"
      @proc[boxname].pid      
    end
    
    def stop(boxname="mmsbox")
      raise "No record of #{boxname} process" if @proc[boxname].nil?

      Process.kill("TERM", @proc[boxname].pid)
      Process.waitpid(@proc[boxname].pid)
      @proc[boxname] = nil
      $?.exitstatus
    end

    # This is a special version of popen which captures stdout, stdin and stdout
    # and the PID of the executing process
    def custom_popen(*cmd)
      pw = IO::pipe   # pipe[0] for read, pipe[1] for write
      pr = IO::pipe
      pe = IO::pipe
      pid_pipe = IO::pipe # pipe for communicating the process id of the started process

      executing_proc_pid = nil

      pid = fork{
        # child
        executing_proc_pid = fork{
          # grandchild
          pw[1].close
          STDIN.reopen(pw[0])
          pw[0].close

          pr[0].close
          STDOUT.reopen(pr[1])
          pr[1].close

          pe[0].close
          STDERR.reopen(pe[1])
          pe[1].close

          exec(*cmd)
        }
        pid_pipe[1].write(executing_proc_pid.to_s + "\n")
        pid_pipe[1].close
        exit!(0)
      }

      pw[0].close
      pr[1].close
      pe[1].close

      executing_proc_pid = pid_pipe[0].gets.to_i
      puts "from pipe: executing_proc_pid=#{executing_proc_pid}"
      pid_pipe[0].close

      Process.waitpid(pid)

      pi = [pw[1], pr[0], pe[0], executing_proc_pid]
      pw[1].sync = true
      if defined? yield
        begin
          return yield(*pi)
        ensure
          pi.each{|p| p.close unless p.closed?}
        end
      end
      pi
    end
  end
end
