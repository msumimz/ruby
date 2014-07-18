load File.expand_path("assertions.rb", File.dirname(__FILE__))

def callee
  1
end

def m
  callee
end

Jit.precompile Object, :m

assert(m == 1)

def callee
  3
end

assert(m == 3)

Jit.precompile Object, :m

assert(m == 3)

