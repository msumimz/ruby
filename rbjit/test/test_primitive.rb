load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  rbjit__bitwise_compare_eq(1, 2)
end

precompile Object, :m

assert(m == false)
