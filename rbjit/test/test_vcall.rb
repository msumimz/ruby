load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  10
end

def m2
  m1
end

def m3
  m2
end

Jit.precompile Object, :m3
assert(m3 == 10)
