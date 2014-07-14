load File.expand_path("assertions.rb", File.dirname(__FILE__))

def callee
  1
end

def m
  callee
end

precompile Object, :m

assert(m == 1)

def callee
  3
end

assert(m == 3)

precompile Object, :m

assert(m == 3)

