load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  1..2 # evaluated by parser
end

def m2
  3...4 # evaluated by parser
end

def m3(a, b)
  a..b
end

def m4(a, b)
  a...b
end

Jit.precompile Object, :m1
Jit.precompile Object, :m2
Jit.precompile Object, :m3
Jit.precompile Object, :m4

assert(m1 == (1..2))
assert(m2 == (3...4))
assert(m3(1, 2) == (1..2))
assert(m4(3, 4) == (3...4))
