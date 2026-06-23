#!/bin/sh

# Tests should be run from the tests directory
if ! test "$(basename "$PWD")" = "t"
then
	        cd t || exit 2
fi

for f in ../*.h
do
	test "$f" = ../config.h ||
		test "$f" = ../funcname1.h ||
		test "$f" = ../funcname2.h &&
		continue
	awk '\
	BEGIN{def=0;ifn=0;};
	nxt {def=$2;nxt=0};
	/^#ifndef/ {ifn=$2;nxt=1};
	!def || !ifn {if (NR==50){print "fn:", FILENAME; exit 1;} next;};
	def && ifn {if (def != ifn){print "def != ifn:",def,ifn; exit 1;}};
	def !~ /W3M_[A-Z]{1,}_H$/ {print "fmt:", def; exit 1;};
	{nextfile;};
	' "$f" || { >&2 echo "$f"; exit 1; }
done
