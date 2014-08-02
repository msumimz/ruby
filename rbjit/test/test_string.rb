load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  "abc"
end

def m2
  'abc'
end

def m3
  "abc#{123}cde#{456}"
end

def m4
  "#{123}abc#{456}#{789}"
end

Jit.precompile Object, :m1
Jit.precompile Object, :m2
Jit.precompile Object, :m3
Jit.precompile Object, :m4

assert(m1 == "abc")
assert(m2 == "abc")
assert(m3 == "abc123cde456")
assert(m4 == "123abc456789")
