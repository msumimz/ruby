load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  "abc"
end

def m2
  'abc'
end

Jit.precompile Object, :m1
Jit.precompile Object, :m2

assert(m1 == "abc")
assert(m2 == "abc")
