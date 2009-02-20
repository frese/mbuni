module MMSLIB
  module SSH
  def self.tunnel(host, localport, remoteport, user=nil)
    if not user.nil?
      user_param = "-l #{user}"
    else 
      user_param = ""
    end

    pid = fork do 
      exec "ssh #{user_param} -L #{localport}:#{host}:#{remoteport} -N #{host}"
    end
    sleep 1 # wait for tunnel to be established 

    # yield control
    yield

    Process.kill("TERM", pid)
    Process.waitpid(pid)
  end
  end
end
