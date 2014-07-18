load File.expand_path("assertions.rb", File.dirname(__FILE__))

alias foobar eval

def inlined
  3
end

def m(s)
  a = inlined
  foobar s
  a + inlined
end

Jit.precompile Object, :m

assert(m("def inlined; 5; end") == 8)
