load File.expand_path("assertions.rb", File.dirname(__FILE__))

X = 10

class Super
  X = 30
end

module Outer
  X = 20
  class C < Super
    def m
      X
    end
  end
end

Jit.precompile Outer::C, :m

assert(Outer::C.new.m == 20)

module Outer
  remove_const :X
  const_set :X, 999
end

assert(Outer::C.new.m == 999)

class Object
  remove_const :X
end

