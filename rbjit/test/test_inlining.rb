load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  def add_one
    add_one1
  end

  def add_one1
    self + 1
  end

end

def m
  2.add_one
end

Jit.precompile Object, :m

assert(m == 3)

