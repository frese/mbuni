require 'webrick'
require 'mmslib'

include MMSLIB
include MMSLIB::MIME

class MMSSaveServlet < HTTPServlet::AbstractServlet
  def initialize(server, savedir)
    super(server)
    puts "MMSSaveServlet loaded"
    @savedir = savedir

    if not File.exists?(@savedir) 
      raise "#{@savedir} does not exist and I could not create it!" unless Dir.mkdir(@savedir)
    end
    
    #raise "Could not cd to #{@savedir}" unless Dir.chdir(@savedir)
  end

  def do_GET(req,res)
    save_mms(req)
    
    res.status = 200
    res.body = "OK\n";
  end
  
  def get_save_dir
    ts = DateTime.now.strftime("%Y-%m-%d %I:%M:%S")
    save_to_dir = @savedir + "/" + ts
    attempt=0
    while File.exists?(save_to_dir + "-#{attempt}")
       attempt += 1
    end
    
    sdir = save_to_dir + "-#{attempt}"
    
    Dir.mkdir(sdir)
    #Dir.chdir(sdir)
    
    sdir
  end
  
  def save_mms(req)
    sdir = get_save_dir
    File.open(sdir + "/request.data", "w") do |f|
      req.each do |header,value|
        f << "#{header}: #{value}\r\n"
      end
      f << "\r\n"
      f << req.body
    end
    
    # Parse MIME Entities in body
    handle_mime(req, sdir)
  end
  
  def handle_mime(req, sdir)
    puts "Content-Type" + req['content-type']
    if req['content-type'] =~ /.*boundary=(.+)$/
      mime_boundary = $1
    else
      raise "No MIME boundary found!"
    end
    
    entity = MIME::Parser.parse(req.body, mime_boundary)
    File.open(sdir + "/" + "mime.raw", "w") { |f| f << req.body }
    entity.elements.each { |element| save_mime_element(element,sdir) }
  end

  def save_mime_element(e,sdir)
    case e.name
      when "images"
        if e.content_type =~ /image\/(.*)/
          extension = $1
        else
          extension = "unknown_image_type"
        end
        file = e.filename
      when "keyword"
        file = "keyword"
        extension = "txt"
      when "text"
        file = e.filename
        extension = "txt"
      when "binary"
        file = e.filename
        extension = "eaif"
      when "other"
        file = e.filename
        extension = "other"
      else # FIXME: If there are more, we will overwrite
        extension = "unknown"
        if not e.filename.nil?
          file = e.filename          
        else
          file = "some_attachment"
        end
    end
    File.open(sdir + "/" + file + "." + extension, "w") do |f|
        f << e.data
    end
  end
  
  alias :do_POST :do_GET
end

