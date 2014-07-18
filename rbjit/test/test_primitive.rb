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

def m14; rbjit__is_embedded(123) end
def m15; rbjit__is_embedded(self) end

def m16; rbjit__test(123) end
def m17; rbjit__test(false) end
def m18; rbjit__test(nil) end

def m19; rbjit__test_not(123) end
def m20; rbjit__test_not(false) end
def m21; rbjit__test_not(nil) end


Jit.precompile(Object, :m1); assert(m1 == true)
Jit.precompile(Object, :m2); assert(m2 == false)
Jit.precompile(Object, :m3); assert(m3 == true)
Jit.precompile(Object, :m4); assert(m4 == false)
Jit.precompile(Object, :m5); assert(m5 == true)
Jit.precompile(Object, :m6); assert(m6 == false)
Jit.precompile(Object, :m7); assert(m7 == true)
Jit.precompile(Object, :m8); assert(m8 == false)
Jit.precompile(Object, :m9); assert(m9 == true)
Jit.precompile(Object, :m10); assert(m10 == false)
Jit.precompile(Object, :m11); assert(m11 == true)
Jit.precompile(Object, :m12); assert(m12 == false)
Jit.precompile(Array, :m13); assert([].m13 == Array)
Jit.precompile(Object, :m14); assert(m14 == true)
Jit.precompile(Object, :m15); assert(m15 == false)
Jit.precompile(Object, :m16); assert(m16 == true)
Jit.precompile(Object, :m17); assert(m17 == false)
Jit.precompile(Object, :m18); assert(m18 == false)
Jit.precompile(Object, :m19); assert(m19 == false)
Jit.precompile(Object, :m20); assert(m20 == true)
Jit.precompile(Object, :m21); assert(m21 == true)
