load File.expand_path("assertions.rb", File.dirname(__FILE__))

# Test an implicitly defined variable
def m
  a = 1
  if a == 2
    b = 10
  else
    20
  end
  b
end

Jit.precompile Object, :m

assert(m == nil)
