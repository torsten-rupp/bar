load "[pwd]/libscanx.so"

set s "This is '\\\"Hel\\'o World!\\\"'"

scanx $s "%s %s %S" a b c

puts $a
puts $b
puts $c

set s1 "abc = ' x '"
set s2 "abc = ''"
set s3 "abc ="

scanx $s1 "abc = %S" t1
scanx $s2 "abc = %S" t2
scanx $s3 "abc = %S" t3

puts "#$t1#"
puts "#$t2#"
puts "#$t3#"
