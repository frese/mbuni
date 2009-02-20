############################################################################
# The MMSLIB package. Include this file to include the entiry package
# Author: <christiantheihave@gmail.com>
############################################################################

# All classes are in the MMSLIB module. Include it to use.
module MMSLIB; end

base_dir = File.dirname(__FILE__)
mmslib_dir = File.join(base_dir, 'lib/')

$: << mmslib_dir 

Dir.glob(File.join(mmslib_dir, '*')) do |m|
    m =  File.basename(m).to_s
    require(m) if m =~ /^(.*).rb$/
end
