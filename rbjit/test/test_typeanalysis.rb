# This file does not contain automatic tests. You should see log to check results.

def m1
  a = 1 + 2
  b = 1 + 2
  a
end

def m2
  a = 1 + 2
  b = a + 2
  b
end

def m3
  a = 1
  if a == 1
    b = 10 + 20
  else
    b = 20
  end
  b
end

def m4
  if true
    a = 1
  else
    a = nil
  end
  a              # a should be a Fixnum
end

def m5
  1 + 2
end

Jit.precompile Object, :m1
Jit.precompile Object, :m2
Jit.precompile Object, :m3
Jit.precompile Object, :m4
Jit.precompile Object, :m5
