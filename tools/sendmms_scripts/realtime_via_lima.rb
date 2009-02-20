#!/usr/bin/env ruby

require 'mmslib'
include MMSLIB
include MMSLIB::DSL

SSH::tunnel("lima.realtime.dk", "12345", "12345", "realtime") do
  sendmms do
    mms {
      from "cth@itu.dk"
      to "what@ever.com"
      contents {
        image "data/air.gif"
        text "This is a sample text"
      }
    }

    via {
      url "http://localhost:12345/"
      username "real"
      password "1234"
    }
  end 
end
