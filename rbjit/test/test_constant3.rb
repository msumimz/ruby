load File.expand_path("assertions.rb", File.dirname(__FILE__))

class C
  remove_const :X if defined? X
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
