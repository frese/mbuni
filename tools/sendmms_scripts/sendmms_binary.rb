require 'mms_sender'
require 'net/http'

# send via local MBuni installation
#sender = MMSSender.new("http://localhost:10001/","tester", "foobar")

# Send via lima -> realtime
#sender = MMSSender.new("http://localhost:12346/","real","1234")

# Send via lima -> tdc
# sender = MMSSender.new("http://localhost:12347/","real","1234")

# Send via Bogota -> iam
sender = MMSSender.new("http://localhost:20010/","air","123")

# Send til Christian
#sender.sendmms(File.open('Sample.mms').read, "004530235242", "cthav")
#sender.sendmms(File.open('Sample.mms').read, "004523737241", "cthav")
#sender.sendmms(File.open('data/Sample.mms').read, "21261244845", "2222")
sender.sendmms(File.open('Sample.mms').read, "christiantheilhave@gmail.com", "2222")

#sender.sendmms(File.open('test.mms').read, "christiantheilhave@gmail.com", "TDC")

# Send til Allan
#sender.sendmms(File.open('test.mms').read, "004529492814", "tdc")

#sender.sendmms(File.open('test.mms').read, "004529492805", "cth")
#sender.sendmms(File.open('test.mms').read, "0021266377384", "21266377384")

