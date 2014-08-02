load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  [1, 2, 3]
end

Jit.precompile Object, :m

assert(m == [1, 2, 3])
