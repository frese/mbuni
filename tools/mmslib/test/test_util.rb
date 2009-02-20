require 'test/unit'

require File.dirname(__FILE__) + '/../mmslib.rb'

include MMSLIB

class UtilTest < Test::Unit::TestCase
  def test_unquote
    str = "\"quoted\""
    assert(str.unquote == "quoted")
  end
  
  def test_trim
      str = "  test "
      assert(str.trim == "test")
  end
  
  def test_trim_with_expr
    str ="\"test\"\""
    assert(str.trim("\"") == "test")
  end
  
  def test_trim!
    str = "   test  "
    str.trim!
    assert(str == "test")
  end
  
  def test_with_newlines
      str = "\n  test \n\n  "
      str.trim!
      assert(str == "test")
  end
end
