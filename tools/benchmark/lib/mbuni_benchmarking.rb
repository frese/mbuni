module MbuniBenchmarking
  include MMSLIB
  BASEDIR=File.expand_path(File.dirname(__FILE__) + "/..")
  TMPDIR=File.expand_path(BASEDIR+"/tmp")
end

require 'graph'
require 'report'
require 'benchmark_suite'
require 'log_analyzer'
require 'mmsbox_module'
require 'mmsbox_module'
require 'mmsc_simulator_module'
require 'test_module'