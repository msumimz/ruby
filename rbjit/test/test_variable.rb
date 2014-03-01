load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  a = 10
  b = 20
  a
end

precompile Object, :m1
assert(m1 == 10)

def m2
  if true
    a = 10
  else
    a = 20
  end
  a
end

precompile Object, :m2
assert(m2 == 10)

def m3
  if false
    a = 10
  else
  end
  a
end

#precompile Object, :m3
assert(m3 == nil)
