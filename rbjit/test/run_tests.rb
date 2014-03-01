Dir.glob("test/test_*.rb") do |f|
  print f, ": "
  load f
  puts
end
