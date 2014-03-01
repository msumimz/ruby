load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m
  1
end

precompile Object, :m

assert(m_orig == 1)
assert(m == 1)
