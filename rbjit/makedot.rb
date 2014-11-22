require "pp"

within_dot = false
dot_lines = []
dot_files = []
File.open("rbjit_debug.log", "r") do |f|
  f.each_line do |line|
    if within_dot
      if /^[\[=]/.match(line)
        within_dot = false
        dot_files.push dot_lines.join("")
        dot_lines.clear
      else
        dot_lines.push line
      end
    else
      if m = /^\[Dot:\s+\d+\s+([0-9a-f]+)\]/.match(line)
        within_dot = true
      end
    end
  end
end

if dot_files.empty?
  puts("dot file not found")
  exit(1)
end

dot = dot_files[-1]
if ARGV.size > 0
  dot = dot_files[ARGV[0].to_i]
end

if !dot
  puts("bad position specified")
  exit(1)
end

File.open("temp.dot", "w") do |f|
  f.write(dot)
end

system("dot -Tpng temp.dot -o temp.png")
