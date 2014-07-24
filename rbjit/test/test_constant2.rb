
load File.expand_path("assertions.rb", File.dirname(__FILE__))

X = 10

module Outer1
  X = 20
  module Outer2
    class C
      X = 30
    end
  end
end

def m
  Outer1::Outer2::C::X
end

Jit.precompile Object, :m
assert(m == 30)

class C
  X = 999
  def m
    ::X
  end
end

Jit.precompile C, :m
assert(C.new.m == 10)
