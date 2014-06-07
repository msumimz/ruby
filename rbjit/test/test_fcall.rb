load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1(a, b)
  a + b
end

def m2
  m1 10, 20
end

precompile Object, :m2
assert(m2 == 30)
