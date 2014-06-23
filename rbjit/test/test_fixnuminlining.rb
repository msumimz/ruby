load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  alias :old_add :+

  def +(other)
    if rbjit__is_fixnum(self) && rbjit__is_fixnum(other) && rbjit__test_not(rbjit__bitwise_add_overflow(self, other))
      rbjit__bitwise_add(self, rbjit__bitwise_sub(other, rbjit__fixnum_to_int(1)))
    else
      self.old_add(other)
    end
  end

end

def m1
  1 + 2
end

precompile Object, :m1

assert(m1 == 3)

def m2(a)
  1 + a + 3
end

precompile Object, :m2

assert(m2(2) == 3)
