BEGIN {
  # print "struct FuncList w3mFuncList[] = {";
  # n = 0;
  print "#define mouse nulcmd"
  print "#define sgrmouse nulcmd"
  print "#define msToggle nulcmd"
  print "#define movMs nulcmd"
  print "#define menuMs nulcmd"
  print "#define tabMs nulcmd"
  print "#define closeTMs nulcmd"
  print "#define chkNMID nulcmd"
}
/^#/ { next }
{
  # print "/*" n "*/ {\"" $1 "\"," $2 "},";
  # n++;
  print "extern void " $2 "();"
} 
END {
  # print "{ NULL, NULL }"
  # print "};"
}
