# Test that AST nodes should be preserved in iseq
load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  1
end

GC.start

10000.times do
  {}
  []
end

GC.start

Jit.dumptree(Object, :m)

# If the program is not crashed, it will succeed
assert(true)
