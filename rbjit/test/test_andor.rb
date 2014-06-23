load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1; true && nil end
def m2; false && true end
def m3; 123 && 456 end
def m4; nil && false end

def m5; true || nil end
def m6; false || true end
def m7; 123 || 456 end
def m8; nil || false end

precompile(Object, :m1); assert(m1 == nil)
precompile(Object, :m2); assert(m2 == false)
precompile(Object, :m3); assert(m3 == 456)
precompile(Object, :m4); assert(m4 == nil)

precompile(Object, :m5); assert(m5 == true)
precompile(Object, :m6); assert(m6 == true)
precompile(Object, :m7); assert(m7 == 123)
precompile(Object, :m8); assert(m8 == false)
