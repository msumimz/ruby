$LOAD_PATH << File.expand_path("../../lib", File.dirname(__FILE__))
require "benchmark"

def m1
  i = 1
  sum = 0
  while i <= 10000
    sum += i
    i += 1
  end
  sum
end

puts Benchmark::CAPTION
puts Benchmark.measure { 100.times { m1 } }

load File.expand_path("../lib/fixnum.rb", File.dirname(__FILE__))

debugbreak
precompile Object, :m1

puts Benchmark.measure { 100.times { m1 } }
