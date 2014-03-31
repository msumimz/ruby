load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1(a, b)
  a + b
end

precompile Object, :m1

assert(m1(10, 20) == 30)
