require 'net/http'
require 'cgi'

module MMSLIB
  class MMSSender < MMSLIB::Gateway
    def headers(to,from)
      {
        "username" => @username,
        "password"=> @password,
        "to" => to,
        "from" => from,
        "subject" => "Test message"
      }
    end

    def send_binary_mms(mms,to,from)
      hdrs = headers(to,from).merge({"content_type" => "application/vnd.wap.mms-message",'content' => mms})
      response = Net::HTTP.post_form URI.parse(@url), hdrs
      puts "Mbuni said: "
      puts response
      response
    end

    def send_mime_encoded_mms(mms,to,from,subject="testmms",direction=nil)
      uri = URI.parse(@url)
      http = Net::HTTP.new(uri.host, uri.port)

      path = uri.path + "?username=#{CGI::escape(@username)}&password=#{CGI::escape(@password)}&to=#{CGI::escape(to)}&from=#{CGI::escape(from)}"
      path += "&subject=#{CGI::escape(subject)}" unless subject.nil?
      path = path + "&mms-direction=MO" if direction.downcase == "incoming" or direction.downcase == "mo"
      
      response,body = http.post(path, mms.get_mime, mms.get_mime_headers.merge(headers(to,from)))
      #puts mms.get_mime
      [ response, body ]
    end

    # Raise exception if the message cannot be sent. Otherwise just returns true.
    def sendmms(mms, to, from)
      if mms.class == MIME::Encode::Packer
        resp, body = send_mime_encoded_mms(mms,to,from).value
      else
        resp, body = send_binary_mms(mms,to,from).value
      end
      resp.value
      body
    end
    alias :send_mms :sendmms
    
    # Raise exception if the message cannot be sent. Otherwise returns response body text.
    def send(msg)
      if not msg.binary.nil?
        resp,body = send_binary_mms(msg.binary, msg.to, msg.from)
      else
        resp,body = send_mime_encoded_mms(msg.mime, msg.to, msg.from, msg.subject, msg.direction)
      end
      resp.value rescue nil
      body
    end
  end
end
