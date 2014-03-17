def m1
  1 + 2
end

def m2
  a = 1
#  b = nil
  if a == 1
    b = 10 + 20
  else
    b = 20
  end
  b
end

precompile Object, :m1

m1
