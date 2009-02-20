require 'mmslib/mimepack.rb'

include MIME::Encode::Packer


packer = Packer.new
packer.add_smil(File.open("sample.smil").read, "<0000>")
packer.add_image(File.open("sample_image.jpg").read, "jpeg", "sample.jpg") 
packer.add_text(File.open("sample_text.txt").read, "sample.txt")
packer.mime_headers.each do |header|
  
end
puts packer.get_mime
