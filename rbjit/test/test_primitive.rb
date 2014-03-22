load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  rbjit__bitwise_compare_eq(1, 2)
end

precompile Object, :m1

assert(m1 == false)

def m2
  rbjit__bitwise_compare_eq(false, false)
end

precompile Object, :m2

assert(m2 == true)
