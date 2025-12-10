#!/bin/sh

# Tests should be run from the tests directory
if ! test "$(basename "$PWD")" = "t"
then
	        cd t || exit 2
fi

! grep '^DEFUN.*{$' ../*.c
