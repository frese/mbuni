#!/usr/local/bin/ruby

# $Id: mail2mms.rb 463 2008-12-09 13:57:55Z pii $ 

##########################################################################
# Load libraries
# Make sure the MMSLIB is in the ruby load path on most realtime servers
require 'logger'
require 'net/smtp'

DEFAULT_MMSLIB_DIR="/home/realtime/mbuni/tools/mmslib"
$: << DEFAULT_MMSLIB_DIR if File.exists? DEFAULT_MMSLIB_DIR
require 'mmslib'
include MMSLIB

if File.exist?('/home/realtime/mbuni/etc/mail2mms.conf.rb')
    require '/home/realtime/mbuni/etc/mail2mms.conf.rb'
else
    require 'sample-config.rb'
end

class SpamException < Exception; end
class LargeEmailException < Exception; end 
class DeliverFailure < Exception; end

# Setup logger:
logfile = File.open(LOGFILE, File::WRONLY | File::APPEND | File::CREAT)
if (LOG_ROTATION and (LOG_ROTATION.to_s != "" or LOG_ROTATION.to_s.downcase != "never"))
  log = Logger.new(logfile, LOG_ROTATION.to_s.downcase)
else
  log = Logger.new(logfile)
end
log.datetime_format = "%Y-%m-%d %H:%M:%S"

# In case of errors, a reply with the error message is sent to the user
def self.send_user_reply(log, error, email_domain, recipient) 
	begin
		if ERROR_MESSAGES[email_domain] and ERROR_MESSAGES[email_domain][error]
			msg = ERROR_MESSAGES[email_domain][error]
		else
			msg = ERROR_MESSAGES["default"][error]
		end
		originator = MAIL_FROM_USER + "@" + email_domain

		smtp = Net::SMTP.new(SMTP_SERVER_ADDR, SMTP_SERVER_PORT)
		smtp.start do |smtp|
			smtp.send_message msg, originator, recipient
		end
		log.info("Mail with error message sent to " + recipient)
	rescue Exception => e
		log.error("Could not send email: " + e.to_s)
	end
end

# explicitly initialize, so they carry over to rescue blocks
email_domain = nil
mail = nil

# The main script:
begin
  MbuniConfiguration.load_global_configuration
  mail_domain = "default"
  delivered = false
  log.info("mail2mms - reading mail from STDIN")
  mail = MMSMail.new
  MMSMail.read_from STDIN, mail
  mail.body.split("\n").each { |line| log.info line }

  mail.recipients.each do |rcpt|
    log.info "Try to deliver to #{rcpt}"
    email_domain = $1 if rcpt =~ /^.*@(.*)$/
    instance = Instance.find_by_domain(email_domain)
    if nil==instance
      log.warn "No instances are setup to handle emails to: " + rcpt
      next
    end
    rcpt =~ VALID_RECIPIENT_ADDRESS or raise SpamException.new("Invalid recipient address (#{rcpt}) - mail dropped")

    if mail.size > MAX_ATTACHMENTS_SIZE
        raise LargeEmailException.new("Email is to big (#{mail.size} bytes). Max is #{MAX_ATTACHMENTS_SIZE} bytes!") 
    end
    
    log.info "Routing email to #{rcpt} via #{instance}"
  
    mms = mail.to_mms(rcpt)
    gateway = instance.gateway_for_user "mail"
    raise DeliverFailure.new("Cannot locate valid user/sendmms-port to send mail in #{instance.name} configuration") unless gateway
    
    gateway = gateway.promote_to MMSSender
    response = gateway.send(mms) # raises exception on error
    raise DeliverFailure.new("Unexpected response from Mbuni: #{response}") unless response =~ /Accepted/
    
    delivered = true
  end
  
  log.warn "Could not deliver email to any of the recipients" unless delivered 

rescue SpamException => e
   log.info "Looks like SPAM: #{e.to_s}"
   exit 0
rescue DeliverFailure => e
   log.error "DeliverFailure: #{e.to_s}"
   if email_domain and mail and mail.originator  
       send_user_reply(log, :service_unavailable,email_domain,mail.originator)
   else
       log.error("Cannot send mail to user because address could not be extracted")
   end
   exit 0 # always tell sendmail that we handled it
rescue LargeEmailException => e 
   log.warn e.to_s
   if email_domain and mail and mail.originator  
       send_user_reply(log, :large_email,email_domain,mail.originator)
   else
       log.error("Cannot send mail to user because address could not be extracted")
   end
   exit 0 # always tell sendmail that we handled it
rescue Exception => e
  log.error e.to_s
  if email_domain.nil? and mail and mail.recipient
     email_domain = $1 if mail.recipient =~ /^.*@(.*)$/ 
  end

  e.backtrace.to_s.split("\n").each { |line| log.error line }
  if email_domain and mail and mail.originator
      send_user_reply(log, :service_unavailable, email_domain, mail.originator) 
  else 
       log.error("Cannot send mail to user because address could not be extracted")
  end

  exit 0 # Pretend to sendmail, that it went ok
end

log.info "Success"

exit 0  # success
