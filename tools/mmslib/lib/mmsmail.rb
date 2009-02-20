require 'rubygems'
require 'tmail'

module MMSLIB
  class MMSMail
    attr_accessor :body, :headers, :recipients, :text_part
    attr_reader :mime_boundary, :text_part
    
    include DSL

    def initialize
      @raw = nil
      @body = ""
      @headers = {}
    end
    
    def self.read_from(file=STDIN, mmsmail = nil)
      mmsmail = MMSMail.new if mmsmail.nil?
      lines = []
      while ((line = file.gets))
        lines << line
      end
      mmsmail.parse(lines)
    end
    
    def parse(lines)
      @raw = lines.join

      # Crude extraction of some headers even if mail is not parseable
      @from = $1 if @raw =~ /^From: (.*?@.*)$/
      @to = $1 if @raw =~ /^To: (.*?@.*)$/
      @headers['Content-Type'] = $1 if @raw =~ /^Content-Type: (.*)$/

      @tmail = TMail::Mail.parse(@raw)      
@raw=nil
      in_body = false
      last_hdr_line = ""
      lines.each do |line|
        if in_body
          @body << line
        elsif line =~ /^\s*$/
          in_body = true
        else # Parse headers
          if line =~ /^\s+(.*)$/
              if last_hdr_line =~ /^(.*):(.*)$/
                key = $1
                @headers[key] += line
              end
          elsif line =~ /^(.*):(.*)$/
            @headers[$1] = $2.trim
            last_hdr_line = line
          end
        end
      end
      parse_headers
      parse_body
      self
    end
    
    def parse_headers
      @mime_boundary = $1 if @headers["Content-Type"] =~ /boundary=\"?(.*)\"?/
      raise "No mime boundary found (#{@headers['Content-Type']})" unless @mime_boundary
      @mime_boundary.trim!('"')
      
      @content_id = @headers['Content-ID']
      @subject = @headers["Subject"]
    end
    
    def parse_body
      part1 = nil
      part2 = @body
      0.upto @body.size-1 do |i|
        if part2[0..1+@mime_boundary.size] == "--#{@mime_boundary}"
          @text_part = part1
          @mime = part2
          break
        end
        part1 = @body[0..i]
        part2 = @body[i..@body.size]
      end
    end
    
    def size
      @body.size
    end

    
    def recipients
      if @tmail.to
           @tmail.to
      else
           @to
      end
    end

    def recipient
      recipients.first
    end

    def originator
      if @tmail.from.first
          @tmail.from.first
      else
          @from
      end
    end

    def originators
      @tmail.from  
    end

    def to_mms(rcpt=nil)
      # find valid recipient
      rcpt = @tmail.to.first if rcpt == nil
      rcpt = rcpt.split('@').first if rcpt =~ /^.*@.*$/
      
      mms {
        to rcpt
        from @tmail.from.first
        subject "mail2mms " + @tmail.subject

        mime_contents {
          boundary @mime_boundary
          content_id @content_id
          raw_mime @mime
        }
        direction :incoming
      }
    end
  end
end
