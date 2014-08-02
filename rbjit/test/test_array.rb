load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  [1, 2, 3]
end

def m2
  a = [1, 2]
  [*a, 3]
end

def m3
  a = [1]
  [*a, 2, 3]
end

def m4
  a = [2, 3]
  [1, *a]
end

def m5
  a = [2]
  [1, *a, 3]
end

def m6
  a = [1]
  b = [*a, 2, 3]
  a.push 4
  b
end

Jit.precompile Object, :m1
Jit.precompile Object, :m2
Jit.precompile Object, :m3
Jit.precompile Object, :m4
Jit.precompile Object, :m5

assert(m1 == [1, 2, 3])
assert(m2 == [1, 2, 3])
assert(m3 == [1, 2, 3])
assert(m4 == [1, 2, 3])
assert(m5 == [1, 2, 3])
assert(m6 == [1, 2, 3])
