load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  def add_one
    self + 1
  end

end

def m
  a = 1 + 2
  a.add_one
end

class Bignum

  def add_one
    self + 1
  end

end

precompile Object, :m

assert(m == 4)

