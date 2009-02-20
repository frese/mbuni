require 'mmslib'

include MMSLIB
include SOAP
include DSL

# Send an DeliverReq as if mmsc
SSH::tunnel("lima.realtime.dk", "14000", "14000", "realtime")  do 
  soap_deliver_request do
    mms {
      # Addresses from comcel come in this strange email format
      from "+573202355840/TYPE=PLMN@mms.comcel.net"
      to "+3456/TYPE=PLMN@mms.comcel.net"
      contents {
        text "Just some test text"
      }
    }
    via {
      url "http://localhost:14000/"
    }
  end
end
