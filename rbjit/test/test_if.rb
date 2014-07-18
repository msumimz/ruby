load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  if 1
    10
  else
    20
  end
end

Jit.precompile Object, :m1
assert(m1 == 10)

def m2
  if true
    10
  else
    20
  end
end

Jit.precompile Object, :m2
assert(m2 == 10)

def m3
  if false
    10
  else
    20
  end
end

Jit.precompile Object, :m3
assert(m3 == 20)

def m4
  if nil
    10
  else
    20
  end
end

Jit.precompile Object, :m4
assert(m4 == 20)

