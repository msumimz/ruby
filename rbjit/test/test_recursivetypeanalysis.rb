# This file does not contain automatic tests. You should see log to check results.

def m(n)
  if n == 1
    1
  else
    m(n - 1)
  end
end

Jit.precompile Object, :m
