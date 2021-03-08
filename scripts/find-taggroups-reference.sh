#!/bin/sh
if [ $# -ne 0 ]
then
	CURDIR="$1"
	shift
else
	CURDIR="."
fi
IDENTIFIER="[-_.[:alnum:]][-_.[:alnum:]]*"
find "$CURDIR" -type f | xargs grep -e "${IDENTIFIER}:${IDENTIFIER}:${IDENTIFIER}" |
	grep -v "/.git/" |
	grep -v "/docs/"
