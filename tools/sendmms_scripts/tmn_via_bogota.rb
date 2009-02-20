#!/usr/bin/env ruby

require 'mmslib'
include MMSLIB
include MMSLIB::DSL

SSH::tunnel("bogota.realtime.dk", "12366", "12366", "realtime") do
  sleep 100
  sendmms do
    mms {
      to "123456789"
      from "12478"
      contents {
        #image "data/air.gif"
        text "Hello"
      }
    }

    via {
      url "http://127.0.0.1:12366/"
      username "real"
      password "1234"
    }
  end 
end
puts "done"

