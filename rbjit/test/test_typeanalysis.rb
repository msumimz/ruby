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

precompile Object, :m2

m2
