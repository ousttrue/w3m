BEGIN {
  # print "#include \"defun.h\""
  # print "#include \"funcheader.h\""
  # print "struct FuncList{const char* name; DefunFunc func;};"
  print "const c = @import(\"../../c_include.zig\").c;";
  print "pub const w3mFuncList = [_] struct{name:[]const u8, func:c.DefunFunc} {";
  n = 0;
}
/^#/ { next }
{
  print ".{ .name=\"" $1 "\", .func=c." $2 "}, //" n;
  n++;
} 
END {
  print "};"
}
