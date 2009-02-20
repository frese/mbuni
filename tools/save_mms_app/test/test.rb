[ 'util', 'mime'].each do |t|
  require File.dirname(__FILE__) + '/test_' + t + ".rb"
end
