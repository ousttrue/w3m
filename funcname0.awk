BEGIN {
  print "#include \"defun.h\""
  print "#include \"funcheader.h\""
  print "struct FuncList{const char* name; DefunFunc func;};"
  print "struct FuncList w3mFuncList[] = {";
  n = 0;
}
/^#/ { next }
{
  print "/*" n "*/ {\"" $1 "\"," $2 "},";
  n++;
} 
END {
  print "{ 0, 0 }"
  print "};"
}
