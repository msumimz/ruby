load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  1
end

Jit.precompile Object, :m

assert(m == 1)

def m
  3
end

assert(m == 3)
