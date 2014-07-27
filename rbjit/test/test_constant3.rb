load File.expand_path("assertions.rb", File.dirname(__FILE__))

if Object.constants(false).include?(:X)
  class Object
    remove_const :X
  end
end
X = 999

class C
  if C.constants(false).include?(:X)
    remove_const :X
  end
  autoload :X, "xxx"

  def m
    X
  end
end

Jit.precompile C, :m

begin
  C.new.m
rescue LoadError
  assert(true)
else
  assert(false)
end
