require 'mmslib'

include MMSLIB
include SOAP
include DSL

# Send an DeliverReq as if mmsc
sendmms do
  mms {
    from "test@oiwejroiwejr.com"
    to "owiejroiwejr@oiwejroiwejr.com"
    contents {
      text "blah blah"	
    }
  }
  gateway {
    url "http://localhost:45454/"
    username "dummy"
    password "dummy"
  }
end
