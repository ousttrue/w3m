BEGIN {
  print "#include \"defun.h\""
}
/^#/ { next }
{
  if (cmd[$2] == "") {
    print "extern void " $2 "(struct DefunContext ctx);"
    cmd[$2] = $2;
  }
} 
END {
}
