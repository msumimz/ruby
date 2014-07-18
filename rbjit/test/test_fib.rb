load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  def fib
    if self <= 1
      self
    else
      (self - 1).fib + (self - 2).fib
    end
  end

end

f = 15.fib

load File.expand_path("../lib/fixnum.rb", File.dirname(__FILE__))

Jit.precompile Fixnum, :fib

assert(15.fib == f)
