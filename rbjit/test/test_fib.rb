load File.expand_path("assertions.rb", File.dirname(__FILE__))

class Fixnum

  def fib
    if self <= 1
      self
    else
      (self - 1).fib + (self - 2).fib
    end
  end

end

f = 15.fib

class Fixnum

  alias :generic_add :+
  alias :generic_add :-
  alias :generic_equals :==
  alias :generic_lt :<
  alias :generic_le :<=

  def +(other)
    if rbjit__is_fixnum(other) && rbjit__test_not(rbjit__bitwise_add_overflow(self, other))
      sum = rbjit__bitwise_add(self, rbjit__bitwise_sub(other, 0))
      rbjit__typecast_fixnum(sum)
    else
      rbjit__typecast_fixnum_bignum(self.generic_add(other))
    end
  end

  def -(other)
    if rbjit__is_fixnum(other) && rbjit__test_not(rbjit__bitwise_sub_overflow(self, other))
      sum = rbjit__bitwise_sub(self, rbjit__bitwise_sub(other, 0))
      rbjit__typecast_fixnum(sum)
    else
      rbjit__typecast_fixnum_bignum(self.generic_sub(other))
    end
  end

  def ==(other)
    if rbjit__bitwise_compare_eq(self, other)
      return true;
    else
      if rbjit__is_fixnum(other)
        return false;
      end
    end
    self.generic_equals(other)
  end

  def <(other)
    if rbjit__is_fixnum(other)
      return rbjit__bitwise_compare_slt(self, other)
    end
    self.generic_lt(other)
  end

  def <=(other)
    if rbjit__is_fixnum(other)
      return rbjit__bitwise_compare_sle(self, other)
    end
    self.generic_le(other)
  end

end

precompile Fixnum, :+
precompile Fixnum, :-
precompile Fixnum, :==
precompile Fixnum, :<
precompile Fixnum, :<=
precompile Fixnum, :fib

assert(15.fib == f)
