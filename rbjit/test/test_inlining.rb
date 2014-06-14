load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  def add_one
    self + 1
  end

end

def m
  1.add_one
end

precompile Object, :m

