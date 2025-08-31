BEGIN {
}
/^#/ { next }
{
  print "{\"" $1 "\"," $2 "},";
} 
END {
}
