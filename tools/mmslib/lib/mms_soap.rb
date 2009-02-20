require 'date'
require 'md5'

require 'mms_dsl.rb'

module MMSLIB
  module SOAP
    class SOAPMessage
      def unique_transaction_id
        MD5.md5(DateTime.now.to_s + Time.now.usec.to_s).to_s
      end

      def timestamp(datetime)
        datetime.strftime("%Y-%M-%dT%H:%M:%SZ")
      end
    end

    class SoapMMSGateway < MMSLIB::Gateway; end

    class MMSDeliverRequest < SOAPMessage
      attr_reader :deliver_req
      
      DEFAULT_FROM="+573133970073/TYPE=PLMN@mms.comcel.net"
      DEFAULT_TO="+3456/TYPE=PLMN@mms.comcel.net"

      def initialize(message, gateway)
        @message, @gateway = message, gateway
        create_deliver_request
      end

      def send
        headers = {}
        headers["username"] = @gateway.username unless @gateway.username.nil?
        headers["password"] = @gateway.password unless @gateway.password.nil?
        
        uri = URI.parse(@gateway.url)
        http = Net::HTTP.new(uri.host, uri.port)
        response,body = http.post(uri.path, @deliver_req.to_s,headers.merge(@deliver_req.hdrs))
        puts "Response from gateway: "
        puts response
        puts body
        response
      end

      def create_deliver_request
        envelope =
        '<?xml version="1.0" ?>' +
        '<env:Envelope xmlns:env="http://schemas.xmlsoap.org/soap/envelope/">' +
        '<env:Header>' + 
        '<mm7:TransactionID xmlns:mm7="http://www.3gpp.org/ftp/Specs/archive/23_series/23.140/schema/REL-5-MM7-1-2" env:mustUnderstand="1">'+
        unique_transaction_id +
        '</mm7:TransactionID>' +
        '</env:Header>' +
        '<env:Body>' +
        '<DeliverReq xmlns="http://www.3gpp.org/ftp/Specs/archive/23_series/23.140/schema/REL-5-MM7-1-2">' +
        '<MM7Version>5.3.0</MM7Version>' +
        '<MMSRelayServerID>MMH1</MMSRelayServerID>' +
        '<Sender><RFC2822Address>' + @message.from + '</RFC2822Address></Sender>' +
        '<Recipients>' + 
        '<To>' + 
        '<RFC2822Address>' + @message.to + '</RFC2822Address>' +
        '</To>' + 
        '</Recipients>' +
        '<TimeStamp>' + timestamp(DateTime.now) + '</TimeStamp>' +
        '<Content href="cid:' +  @message.mime.id + '" />' +
        '</DeliverReq>' +
        '</env:Body>' +
        '</env:Envelope>'
        
        soap_mime = MIME::Encode::Packer.new("</soap-env/start>", "--soap-border","multipart/related")
        soap_mime.add_xml(envelope, "</soap-env/start>")
        soap_mime.add_mime(@message.mime)
        @deliver_req = soap_mime
      end
      
      def to_s
        @deliver_req.to_s
      end
    end

    class SoapResponse < SOAPMessage
      def initialize(msg_id, status, transactionid=nil)
        @message_id, @transactionid, @status = msg_id, transactionid, status
      end

      def self.success(msg_id, transactionid=nil)
        self.new(msg_id, transactionid,1000).to_s
      end

      def self.failure(msg_id, transactionid=nil)
        self.new(msg_id, transactionid,3000).to_s
      end

      def to_s
        msg = MIME::Packer.new
        msg.add(envelope)
        msg
      end

      def status_text(code)
        if code.to_s =~ /^1.*/
          "Success"
        elsif code.to_s =~ /^2.*/
          "Client error"
        elsif code.to_s =~ /^3.*/
          "Server error"
        elsif code.to_s =~ /^4.*/
          "Service error"
        else
          "Unknown (invalid) error"
        end
      end

      def envelope
        @transactionid = uniq_transaction_id if @transactionid.nil?

        envelope = 
        '<env:Envelope xmlns:env="http://schemas.xmlsoap.org/soap/envelope/">' + 
        '<env:Header>' +
        '<mm7:TransactionID xmlns:mm7=”http://www.3gpp.org/ftp/Specs/archive/23_series/23.140/schema/REL-5-MM7-1-4”<p></p> env:mustUnderstand=”1”>' +
        transactionid +
        '</mm7:TransactionID>'
        '</env:Header>' + 
        '<env:Body>' + 
        '<mm7:SubmitRsp xmlns:mm7="http://www.3gpp.org/ftp/Specs/archive/23_series/23.140/schema/REL-5-MM7-1-4”>' +
        '<MM7Version>5.83.0</MM7Version>' +
        '<Status>' +
        '<StatusCode>' + @status + '</StatusCode>' +
        '<StatusText>' + status_text(@status) + '</StatusText>' +
        '</Status>' +
        '<MessageID>' + @message_id + '</MessageID>' +
        '<mm7:SubmitRsp>' +
        '</env:Body>' +
        '</env:Envelope>'
      end
    end
  end
end
