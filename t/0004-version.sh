#!/bin/sh

# Check that version information is the same everywhere.

# Makefile:W3M_VERSION = 0.5.5
# NEWS:w3m 0.5.5 - 2025-08-20

# doc-jp/README:  w3m: WWW wo Miru Tool  version 0.5.5    Jan 15, 2011
# doc/README:     w3m: WWW wo Miru Tool  version 0.5.5    Jan 15, 2011

# w3mhelp-lynx_en.html: (WWW-wo-Miru)  Version 0.5.5 by
# w3mhelp-lynx_ja.html: (WWW-wo-Miru) Version 0.5.5 by
# w3mhelp-w3m_en.html: (WWW-wo-Miru)  Version 0.5.5 by
# w3mhelp-w3m_ja.html: (WWW-wo-Miru) Version 0.5.5 by

# doc-de/w3m.1:.TH W3M 1 2025\-06\-07 "w3m 0.5.5"
# doc-jp/w3m.1:.TH W3M 1 "2025\-06\-07" "w3m 0.5.5"
# doc/w3m.1:.TH W3M 1 "2025\-06\-07" "w3m 0.5.5"

# This test should be run from the repo root
test "${PWD##*/}" != t || cd .. || exit 2

if test "$DEBUG"
then
	unset Q
else
	Q=-q
fi

report()
{
	>&2 printf "Version not found in %s\n" "$1"
	err=1
}

exp=$(sed -n '/define W3M_VERSION/s|.*"w3m/\(.*\)".*$|\1|p' version.h)
test "$exp" || { >&2 printf "%s\n" "Could not get expected version!"; exit 3; }

for f in Makefile
do
	grep $Q "W3M_VERSION = $exp" $f || report $f
done

for f in \
	doc/README \
	doc-jp/README \
	w3mhelp-lynx_en.html \
	w3mhelp-lynx_ja.html \
	w3mhelp-w3m_en.html \
	w3mhelp-w3m_ja.html
do
	grep $Q -i "version $exp" $f || report $f
done

for f in \
	doc/w3m.1 \
	doc-de/w3m.1 \
	doc-jp/w3m.1
do
	grep $Q -i "w3m $exp" $f || report $f
done

exit ${err:-0}
