class String
  def trim!(expr="\s|\n|\r")
    self[0..-1] = self.trim(expr)
  end
  
  def trim(expr="\s|\n|\r")
    tmp = $2.reverse if self =~ /^(#{expr})*(.*)$/
    tmp = $2.reverse if tmp =~ /^(#{expr})*(.*)$/
  end
  
  def unquote
    if self =~ /^\s*\"\s*(.*)\s*\"\s*$/
      $1
    else
      self
    end
  end
end

class Fixnum
  def bytes
    self
  end
  
  def kilobytes
    self*1024
  end
  
  def megabytes
    self.kilobytes * 1024
  end
  
  def gigabytes
    self.megabytes * 1000
  end
  
  def seconds
    self
  end
  
  def minuttes
    self*60
  end
  
  def hours
    60*minuttes
  end
  
  def days
    24*hours
  end
end

def flowerline(of="#",size=80)
  puts of*size
end

def flowerbox(text="", of="#")
  flowerline(of)
  puts "#{of} #{text}"
  flowerline(of)
end