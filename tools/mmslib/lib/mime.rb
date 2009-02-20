module MMSLIB
  module MIME
    module Encode
      require 'base64'

      class MIMEElement
        attr_accessor :content_type, :data, :content_transfer_encoding, :content_id
        def initialize(data,id=nil)
          @content_type = "application/octet-stream"
          @data = data
          @content_transfer_encoding = nil
          @content_id = id
        end
      end

      class TextElement < MIMEElement
        def initialize(data, id)
          super
          @content_type = "text/plain"
        end
      end

      class XMLElement < MIMEElement
        def initialize(data,id)
          super
          @content_type = "text/xml"
        end
      end

      class ImageElement < MIMEElement
        def initialize(image_data, image_type, id)
          @content_type = "image/#{image_type}"
          @data = Base64.encode64(image_data)
          @content_transfer_encoding = "base64"
          @content_id = id unless id.nil?
        end
      end

      class SMILElement < MIMEElement
        def initialize(smil,id)
          super
          @content_type = "application/smil; name=main.smil"
          @content_transfer_encoding = "7bit"
        end
      end
      
      class EmbeddedMimeElement < MIMEElement
        def initialize(mime)
          @data = mime.to_s
          @content_type = mime.hdrs["Content-Type"]
          @content_id = mime.hdrs["Content-ID"]
        end
      end

      class Packer
	      attr_accessor :elements, :boundary, :id, :raw_mime

        def initialize(id="samplemms", boundary="boundary-076438418622afdd6b26275ec582cbb5", content_type=nil)
          @raw_mime = nil
          @elements = []
          @boundary, @id = boundary, id
          @content_type = content_type
          @content_type ||= "multipart/form-data"
        end

        def add_element(elem)
          @elements << elem
        end
        
        def add_mime(elem)
          raise "Not a valid embedded MIME element" unless elem.kind_of?(Packer)
          add_element(EmbeddedMimeElement.new(elem))
        end
        
        def add_first(elem)
          @elements.insert(0,elem)
        end

        def add_text(text, id=nil)
          @elements << TextElement.new(text, id)
        end

        def add_xml(xml,id=nil)
          @elements << XMLElement.new(xml,id)
        end

        def add_image(image_data, image_type, id=nil)
          @elements << ImageElement.new(image_data, image_type,id)
        end

        def add_smil(smil,id="main.smil")
          @elements << SMILElement.new(smil,id)
        end

        def get_mime_headers
          { "MIME-version" => "1.0", "Content-Type" => "#{@content_type}; boundary=#{@boundary}", "Content-ID" => "<#{@id}>" }
        end
        alias :hdrs :get_mime_headers

        def get_mime
          return @raw_mime unless @raw_mime.nil?

          mimestr = ""
          @elements.each do |e|
            next if e.data.nil?
            mimestr << "--#{@boundary}\n"
            mimestr << "Content-Type: #{e.content_type}\n"
            mimestr << "Content-transfer-encoding: #{e.content_transfer_encoding}\n" unless e.content_transfer_encoding.nil?
            mimestr << "Content-ID: #{e.content_id}\n" unless e.content_id.nil?
            mimestr << "\n" + e.data + "\n"
          end
          @raw_mime = mimestr << "--#{@boundary}--\n"
        end
        
        alias :to_s :get_mime
        
        def get_mime_with_hdrs
          mimestr = ""
          get_mime_headers.each_pair do |key,val|
            mimestr << key + ": " + val + "\n"
          end
          mimestr << get_mime
        end
        
        def first_content_id
          @elements.first.content_id
        end
      end
    end

    module Decode
      class Element
        attr_accessor :data, :properties

        def initialize(properties = {}, data = nil)
          @properties, @data = properties, data
        end

        def content_type
          @properties['Content-Type']
        end

        def content_disposition
          @properties['Content-Disposition']
        end

        def content_disposition_find(key_pattern)
          return nil if content_disposition.nil?
          values = content_disposition.split(/;/)
          values.each do |v|
            if v =~ /#{key_pattern}=(.*)/
              return ($1).unquote
            end
          end
          nil
        end

        def filename
          content_disposition_find "filename"
        end

        def name
          content_disposition_find "name"
        end
      end

      # An Entity contains a number of MIME elements
      class Entity
        attr_accessor :elements

        def initialize
          @elements = []
        end

        def add(element)
          @elements << element
        end
      end

      # The parser create an Entity from some raw MIME text
      class Parser
        def self.parse(data, boundrary)
          entity = Entity.new
          parts = data.split("--#{boundrary}")

          parts.each do |part|
            next if part.chomp == "" or part.chomp == "--"
            element = parse_element(part)
            entity.add element unless element.nil?
          end
          
          entity
        end

        def self.parse_element(data)
          # Read headers
          headers, body = data.split("\r\n\r\n")
          Element.new(parse_headers(headers), body)
        end

        def Parser.parse_headers(headers_str)
          hsh = {}
          headers = headers_str.split("\r\n")
          headers.each do |h|
            next if h.chomp == "" or h.chomp == "--"
            name, value = h.split(":")
            name,value = name.trim, value.trim
            hsh[name] = value
          end

          hsh
        end
      end
    end
  end
end
