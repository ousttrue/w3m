BEGIN {
  # print "struct FuncList w3mFuncList[] = {";
  # n = 0;
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
