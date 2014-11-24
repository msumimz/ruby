repeats = ARGV.size == 1 ? ARGV[0].to_i : 1

repeats.times do
  Dir.glob("test/test_*.rb") do |f|
    print f, ": "
    STDOUT.flush # ruby and rbjit's debugprint don't share file buffers
    load f
    puts
  end
end
