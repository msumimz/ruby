load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1; rbjit__bitwise_compare_eq(false, false) end
def m2; rbjit__bitwise_compare_eq(1, 2) end
def m3; rbjit__is_fixnum(123) end
def m4; rbjit__is_fixnum(self) end
def m5; rbjit__is_symbol(:abc) end
def m6; rbjit__is_symbol(1234) end
def m7; rbjit__is_true(true) end
def m8; rbjit__is_true(false) end
def m9; rbjit__is_false(false) end
def m10; rbjit__is_false(true) end
def m11; rbjit__is_nil(nil) end
def m12; rbjit__is_nil(self) end
class Array; def m13; rbjit__class_of(self) end; end

precompile(Object, :m1); assert(m1 == true)
precompile(Object, :m2); assert(m2 == false)
precompile(Object, :m3); assert(m3 == true)
precompile(Object, :m4); assert(m4 == false)
precompile(Object, :m5); assert(m5 == true)
precompile(Object, :m6); assert(m6 == false)
precompile(Object, :m7); assert(m7 == true)
precompile(Object, :m8); assert(m8 == false)
precompile(Object, :m9); assert(m9 == true)
precompile(Object, :m10); assert(m10 == false)
precompile(Object, :m11); assert(m11 == true)
precompile(Object, :m12); assert(m12 == false)
precompile(Array, :m13); assert([].m13 == Array)
