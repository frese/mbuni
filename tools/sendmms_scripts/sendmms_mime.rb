require 'mimepack.rb'
require 'mms_sender'
require 'net/http'

include MIME::Encode::Packer

# Create a super simple MIME message with just a text message in it
def msg1
  pack = Packer.new
  #pack.add_image(File.open("sample_image.jpg").read, "jpeg", "sample.jpeg") 
 # pack.add_smil(File.open("sample.smil").read)  
#  pack.add_text("File.open("sample_text.txt").read"vir, "sample1.txt")
  #pack.add_text(File.open("sample_text.txt").read, "sample2.txt")  
  pack.add_text("view ", "view.txt");
  pack
end

# Create a multi part mime message with smil, text and image (jpeg) content
def msg2
  pack = Packer.new
#  pack.add_smil(File.open("sample.smil").read)
  pack.add_image(File.open("data/sample_image.jpg").read, "jpeg", "sample_image.jpeg")
#  pack.add_text(File.open("sample_text.txt").read, "sample_text.txt")
  pack
end

# Create a multi part mime message with smil, text and image (gif) content
def msg3
  pack = Packer.new
  pack.add_image(File.open("data/air.jpeg").read, "jpeg", "air.jpg")
  pack.add_text("Test MMS from Atchik-Realtime.", "sample.txt")
  pack
end

# Send via lima -> tdc
#sender = MMSSender.new("http://localhost:12346/","real","1234")
#sender = MMSSender.new("http://localhost:12346/","real","1234")
#sender = MMSSender.new("http://localhost:10001", "tester","foobar")
#sender = MMSSender.new("http://localhost:20010/","air","123")
#sender = MMSSender.new("http://localhost:20010/","air","123")
sender = MMSSender.new("http://localhost:45454/","dummy","dummy")
#sender = MMSSender.new("http://localhost:45454/","air","123")

# Send til Allan
#sender.sendmms(msg3, "004529492814", "air")

# Send til Christian
#sender.sendmms(msg3, "004530235242", "air")
# "21266377384"
sender.sendmms(msg3, "21221261244845", "2222")
#sender.sendmms(msg1, "2222","21261244845")
