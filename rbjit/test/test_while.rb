load File.expand_path("assertions.rb", File.dirname(__FILE__))

def m1
  i = 1
  sum = 0
  while i <= 10
    sum += i
    i += 1
  end
  sum
end

Jit.precompile Object, :m1

assert(m1 == 55)
