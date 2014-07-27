load File.expand_path("assertions.rb", File.dirname(__FILE__))

# This file will spend a lot of time to load
LOAEDED_FILE = File.expand_path("constant4_autoloadfile.rb", File.dirname(__FILE__))

class C
  if C.constants(false).include?(:X)
    remove_const :X
  end
  autoload :X, LOAEDED_FILE

  def m
    X
  end
end

Jit.precompile C, :m

th = Thread.new do
  assert(C.new.m == 10)
end

assert(C.new.m == 10)

th.join
