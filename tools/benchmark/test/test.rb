require 'test/unit'

Dir.new(File.dirname(__FILE__)).each do |f|
	require File.dirname(__FILE__) +"/"+ f unless f =~ /^\..*/ or f == __FILE__ 
end
