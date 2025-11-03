BEGIN {
  print "#pragma once"
}
/^#/ { next }
a[$2]++ == 0 {
  print "void " $2 "();";
}
