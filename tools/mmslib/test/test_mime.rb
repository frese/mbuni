require 'test/unit'
require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB

class MIMETest < Test::Unit::TestCase
	def setup
		@ex1 = File.open(File.dirname(__FILE__) + "/mime.file").read
	end	
	
	def keyword_element
	  MIME::Decode::Element.new(
	    { 
	      "Content-Type" => "text/plain",
	      "Content-Disposition" => "form-data; name=\"keyword\" "
	    },
	    "blah\n"
	  )
	end
	
	def image_element
	  MIME::Decode::Element.new(
	    {
	      "Content-Type"=>"image/jpeg",
	      "Content-Disposition"=>"form-data; name=\"images\"; filename=\"image.jpg\""
	    },
	    "IMAGEDATAXYZ!341!%qwe\n"
	  )
	end
	
	def text_element
	  MIME::Decode::Element.new(
	    { 
        "Content-Type"=>"text/plain; charset=us-ascii",
        "Content-Disposition"=>"form-data; name=\"text\"; filename=\"file.txt\""
      },
	    "This is a sample text file\nblAH\n"
	  )
  end
	
	def test_element_keyword
    e = keyword_element
	  assert(e.content_type == "text/plain")
	  assert(e.name == "keyword")
	  assert(e.data == "blah\n")
	end
	
  def test_element_image
    e = image_element
    assert(e.content_type == "image/jpeg")
    assert(e.filename == "image.jpg")
    assert(e.name == "images")
    assert(e.data == "IMAGEDATAXYZ!341!%qwe\n")
  end
  
  def test_text_element
    e = text_element
    assert(e.content_type =~ /text\/plain/) # May also have encoding
    assert(e.filename == "file.txt")
    assert(e.name == "text")
    assert(e.data == "This is a sample text file\nblAH\n")
  end

	def test_mime_parse
		entity = MIME::Decode::Parser.parse(@ex1, "_boundary_1282736013_1221482465_X_l_bd833063937")
		assert(entity.class == MIME::Decode::Entity)
		assert(entity.elements.size == 4)
  end
end
