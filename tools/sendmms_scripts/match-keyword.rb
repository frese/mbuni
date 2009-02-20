require 'mmslib'

include MMSLIB
include SOAP
include DSL

sendmms do
  mms {
    from "test@oiwejroiwejr.com"
    to "owiejroiwejr@oiwejroiwejr.com"
    contents {
      text "foo - match keyword"	
    }
    direction :incoming
  }
  gateway {
    url "http://localhost:45454/"
    username "dummy"
    password "dummy"
  }
end
