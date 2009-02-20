##########################################################################
# Settings
##########################################################################

# Where to write the log
LOGFILE="/home/realtime/mbuni/log/mail2mms.log"

# When to rotate logs. Options are: daily, weekly, monthly and never
LOG_ROTATION=:daily

# The maximal sizes of of the email in bytes (not including headers)
MAX_ATTACHMENTS_SIZE=1.megabytes

# If more than SCRIPT_TIMEOUT seconds pass the script will be terminated prematurely
# and exit non-zero return code
SCRIPT_TIMEOUT=10.seconds

# Sending reply mails:
SMTP_SERVER_ADDR="localhost"
SMTP_SERVER_PORT=25
MAIL_FROM_USER="service"

# A regular expression for validating to the recipient address 
VALID_RECIPIENT_ADDRESS = /^[A-Za-z]{2}[0-9]{4}@.*$/ # two letters, and four digits @ domain.com

# Error messages, configurable per domain where the email was sent to 
# If a domain is not matched, the "default" error messages will be used
ERROR_MESSAGES = {
	"default" => {
		:large_email => "email rejected",
		:service_unavailable => "service unavailable"
	},

	"chatair.mobi" => {
		:large_email => "The system cannot process your request because the email you sent is to large.",
		:service_unavailable => "Service unavailable. Please try again later."
	},

	"chatair.dk" => {
		:large_email => "Din er email er for stor",
		:service_unavailable => "Systemet kunne ikke håndtere din email lige nu. Prøv igen senere",
	},

	"chatair.blah" => {
		:large_email => "blah di blah blah blah",
		:service_unavailable => "blah blah !!"
	}
}

