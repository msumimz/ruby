class Fixnum

  alias :generic_add :+
  alias :generic_add :-
  alias :generic_equals :==
  alias :generic_lt :<
  alias :generic_le :<=
  alias :generic_gt :>
  alias :generic_ge :>=

  def +(other)
    if rbjit__is_fixnum(other) && rbjit__test_not(rbjit__bitwise_add_overflow(self, rbjit__bitwise_sub(other, 0)))
      sum = rbjit__bitwise_add(self, rbjit__bitwise_sub(other, 0))
      rbjit__typecast_fixnum(sum)
    else
      rbjit__typecast_fixnum_bignum(self.generic_add(other))
    end
  end

  def -(other)
    if rbjit__is_fixnum(other) && rbjit__test_not(rbjit__bitwise_sub_overflow(self, rbjit__bitwise_sub(other, 0)))
      sum = rbjit__bitwise_sub(self, rbjit__bitwise_sub(other, 0))
      rbjit__typecast_fixnum(sum)
    else
      rbjit__typecast_fixnum_bignum(self.generic_sub(other))
    end
  end

  def ==(other)
    if rbjit__bitwise_compare_eq(self, other)
      return true
    else
      if rbjit__is_fixnum(other)
        return false
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

  def >(other)
    if rbjit__is_fixnum(other)
      return rbjit__bitwise_compare_sgt(self, other)
    end
    self.generic_gt(other)
  end

  def >=(other)
    if rbjit__is_fixnum(other)
      return rbjit__bitwise_compare_sge(self, other)
    end
    self.generic_ge(other)
  end

end

Jit.precompile Fixnum, :+
Jit.precompile Fixnum, :-
Jit.precompile Fixnum, :==
Jit.precompile Fixnum, :<
Jit.precompile Fixnum, :<=
Jit.precompile Fixnum, :>
Jit.precompile Fixnum, :>=

