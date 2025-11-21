#!/bin/sh

total=0
pass=0
fail=0
w3m="../w3m -config /dev/null -o ignore_null_img_alt=false"
w3m="$w3m -I utf-8 -O utf-8 -T text/html"

test "$@" || set -- expecteds/*.html

for f
do
	test -f "$f" || f="$f".html
	cmd="$w3m -I utf-8 -O utf-8 -T text/html"
	opts="${f%.html}".opts
	test -f "$opts" && cmd="$cmd $(grep -v '^#' "$opts")"
	if $cmd < "$f" |
		diff -u - "${f%.html}".expected
	then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
		exit 1
	fi
	total=$((total + 1))
done

echo
echo "TOTAL: $total test(s)"
echo "PASS : $pass"
echo "FAIL : $fail"
test 0 -eq "$fail"
