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

precompile Object, :m1

Benchmark.bm do |bm|
  bm.report("interpreted") { 100.times { m1_orig } }
  bm.report("precompiled") { 100.times { m1 } }
end
