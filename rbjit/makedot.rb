require "pp"

within_dot = false
cfg_name = nil
dot_lines = []
dot_files = {}
File.open("rbjit_debug.log", "r") do |f|
  f.each_line do |line|
    if within_dot
      if /^[\[=]/.match(line)
        within_dot = false
        dot_files[cfg_name] = dot_lines.join("")
        dot_lines.clear
      else
        dot_lines.push line
      end
    else
      if m = /^\[Dot: ([0-9a-f]+)\]/.match(line)
        within_dot = true
        cfg_name = m[1]
      end
    end
  end
end

if dot_files.empty?
  puts("not dot file found")
  exit(1)
end

dot = dot_files.values[-1]
if ARGV.size > 0
  if m = /^(-\d+)/.match(ARGV[0])
    dot = dot_files.values[m[1].to_i]
  else
    dot = dot_files[ARGV[0]]
  end
end

if !dot
  puts("illegal position")
  exit(1)
end

File.open("test.dot", "w") do |f|
  f.write(dot)
end

system("dot -Tpng test.dot -o test.png")
