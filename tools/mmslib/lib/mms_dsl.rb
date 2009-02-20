# A Simple DSL for creating and sending MMS messages through Mbuni

module MMSLIB
  
  # These types are used by various parts of the module
  class MMS
    attr_accessor :from, :to, :mime, :direction, :binary, :subject
    def initialize(to=nil,from=nil)
      @to, @from = to, from
      @mime = MIME::Encode::Packer.new
      @direction = "outgoing"
      @binary = nil
      @subject = nil
    end
  end

  class Gateway
    attr_accessor :url, :username, :password
    
    def initialize(url=nil, username=nil, password=nil)
      @url, @username, @password = url, username, password
    end
    
    def promote_to(g)
      g.new(@url,@username,@password)
    end
  end
  
  module DSL
    attr_reader :inside_mms, :inside_gateway, :inside_send, :inside_contents, :inside_mime_contents, :inside_mmsc, :port
    
    def mmsc 
      @inside_mmsc = true
      yield
      raise "mmsc declaration is missing url" if @mmsc_url.nil?
      @mmsc = MMSCSimulator::MMSCSimulator.new(@mmsc_url, @mmsc_type)
      @mmsc_pid = @mmsc.start
      trap "INT" do
        Process.kill @mmsc_pid
        Process.waitpid(@mmsc_pid)
      end
    end
    
    def type(type)
      dsl_assert(:inside_mmsc)
      @mmsc_type = type
    end
 
# Defined implicitly via url   
#    def port(p)
#      raise "port outside mmsc declaration" unless @inside_mmsc
#      @mmsc_port = p
#    end

# Define later
#    def handle(msgtype, &blck)
#      blck.call
#    end
    
    def sendmms
      @inside_send = true
      @messages = []
      @gateway = nil        
    
      begin
        yield 
        raise "No messages declared!" if @messages.empty?
        raise "No gateway specified! (specify with via)" if @gateway.nil?
        @gateway = @gateway.promote_to(MMSSender)
        @messages.each do |msg|
          puts "Sending message from #{msg.from} to #{msg.to}..."
          @gateway.send(msg)
        end
        @inside_send = false
      rescue Exception => boom
        puts boom.to_s
        puts boom.backtrace
      end
    end
    
    def soap_deliver_request
      @inside_send = true
      @messages = []
      @gateway = nil
      
      begin
        yield 
        raise "No messages declared!" if @messages.empty?
        raise "No gateway specified!" if @gateway.nil?
        @messages.each do |msg|
          deliver_req = SOAP::MMSDeliverRequest.new(msg, @gateway)
          puts "Sending SOAP DeliveryReq gateway.."
          deliver_req.send
        end
        @inside_send = false
      rescue Exception => boom
        puts boom.to_s
        puts boom.backtrace
      end
    end

    def mms
      #dsl_assert(:inside_send)
      @inside_mms = true
      @current_mms = MMS.new()
      yield 
      @messages << @current_mms unless @messages.nil?
      @inside_mms = false
      @current_mms
    end

    def gateway
      #dsl_assert(:inside_send)
      @inside_gateway = true
      @gateway = Gateway.new(nil,nil,nil) # Initialize placeholder
      yield
      @inside_gateway = false
      @gateway
    end
    alias :via :gateway

    def username(u)
      dsl_assert(:inside_gateway)
      @gateway.username = u
    end

    def password(p)
      dsl_assert(:inside_gateway)
      @gateway.password = p
    end

    def url(u)
      if inside_gateway
        @gateway.url = u
      elsif inside_mmsc
        @mmsc_url = u        
      end
    end

    def to(to)
      dsl_assert(:inside_mms)
      @current_mms.to=to;
    end
    
    def subject(subj)
      dsl_assert :inside_mms
      @current_mms.subject = subj
    end
    
    def direction(dir)
      dsl_assert(:inside_mms)
      raise "Invalid direction specified: Must be incoming or outgoing" unless 
        ["incoming","outgoing","mo","mt"].include?(dir.to_s.downcase)
      @current_mms.direction = dir.to_s.downcase.to_s
    end

    def from(from)
      dsl_assert(:inside_mms)
      @current_mms.from = from
    end

    def contents(mime=nil)
      dsl_assert :inside_mms
      if mime.nil?
        @inside_contents = true
        yield
        @inside_contents = false
      else # Used for providing the content as raw mime
        @current_mms.mime.set_raw_mime(mime)
      end
    end
    
    def mime_contents
      dsl_assert :inside_mms
      @inside_mime_contents = true
      yield
      @inside_mime_contents = false
    end
    
    def boundary(mime_boundary)
      dsl_assert :inside_mime_contents
      @current_mms.mime.boundary = mime_boundary
    end
    
    def content_id(id)
      dsl_assert :inside_mime_contents
      @current_mms.mime.id = id
    end
    
    def raw_mime(str)
      dsl_assert :inside_mime_contents
      @current_mms.mime.raw_mime = str  
    end
    
    def from_binary(binary_file)
      dsl_assert(:inside_mms)
      @current_mms.binary = File.open(binary_file).read
    end

    def image(image_file)
      dsl_assert(:inside_contents)
      @current_mms.mime.add_image(File.open(image_file).read, File.basename(image_file))
    end

    def smil(smil_file)
      dsl_assert(:inside_contents)
      @current_mms.mime.add_smil(File.open(smil_file).read, File.basename(smil_file))
    end

    def text(text)
      dsl_assert(:inside_contents)
      if File.exist?(text)
        @current_mms.mime.add_text(File.open(text).read, File.basename(text))
      else
        @current_mms.mime.add_text(text, 
        "file" + @current_mms.mime.elements.length.to_s + ".txt")
      end
    end

    def dsl_assert(symbol)
      if not (self.send(symbol))
        raise "Cannot set #{caller.gsub(/.*:in `(.*)'/, "#{$1}")} outside #{symbol.to_s} declaration\n"
      end
    end

  end # module DSL
end # module MMSLIB
