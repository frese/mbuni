require 'test/unit'

require 'test/unit'
require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB

class TestMMSMail < Test::Unit::TestCase
  def test_001_parse_email
    mail = MMSMail.read_from(File.open("fixtures/test.email", "r"))
    assert(nil != mail, "mail not nil")
    assert(mail.headers["Subject"] == "News for today", "value of subject")
    assert(mail.headers["To"] == "+447778889999/TYPE=PLMN@mms.domain.com", "value of to: #{mail.headers['To']}")
    assert(mail.headers["From"] == "username@domain.com", "value of from")
    assert(mail.mime_boundary == "StoryParts-74526-8432-2002-77645", "value of mime_boundary: [#{mail.mime_boundary}]")
    assert(mail.body != nil, "Body not nil")
    assert(mail.body.size > 0, "Body not empty")
  end
  
  def test_002_mail2mms
    mail = MMSMail.read_from(File.open("fixtures/test.email", "r"))
    mms = mail.to_mms
    assert(nil != mms, "mms not nil")
    assert(mms.to == "+447778889999/TYPE=PLMN", "domain is stripped in mms to addr #{mms.to}")
    assert(mms.from == "username@domain.com", "value of from: #{mms.from}")
    assert(mms.subject == "mail2mms News for today", "value of subject: #{mms.inspect}")
    assert(nil != mms.mime, "mms.mime not nil")
    assert(mms.mime.boundary == mail.mime_boundary, "mime boundary is preserved")
    assert(mms.mime.id == "<SaturnPics-01020930@news.tnn.com>", "mime content-id is preserved")
  end
  
  def test_003_validity_of_mime
    mail = MMSMail.read_from(File.open("fixtures/test.email", "r"))
    mms = mail.to_mms
    puts mms.inspect
    
    entity = MIME::Decode::Parser.parse(mms.mime.to_s, mms.mime.boundary)
    assert(entity != nil, "entity not nil")
    puts entity.elements.inspect
    
    assert(entity.elements.size == 2, "parsed mime has two elements")
    assert(entity.elements[0].content_type == "text/plain; charset=\"us-ascii\"", "content-type of first mime elem: #{entity.elements[0].content_type}")
    assert(entity.elements[1].content_type == "image/gif")
  end
  
  def test_004_outlook_mail
    mail = MMSMail.read_from(File.open("fixtures/outlook.email", "r"))
    puts mail.text_part.trim
    assert(mail.text_part.trim == "This is a multipart message in MIME format.", "Text part is correct")
    mms = mail.to_mms
    assert(mms.mime.to_s.size > 0)
  end
  
  def test_005_mbuni_mail
    mail = MMSMail.read_from(File.open("fixtures/mbuni.email", "r"))
    mms = mail.to_mms
    assert(mms.mime.to_s.size > 0)
  end
  
  
  def test_006_mbuni_mail
    mail = MMSMail.read_from(File.open("fixtures/gmail.email", "r"))
    mms = mail.to_mms
    assert(mms.mime.to_s.size > 0)
  end
end