load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum
  def add(a)
    self + a
  end
end

Jit.precompile Fixnum, :add

assert(10.add(20) == 30)
