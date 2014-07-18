load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  1 + 2
end

Jit.precompile Object, :m

assert(m == 3)
