#!/bin/sh
if [ $# -ne 0 ]
then
	CURDIR="$1"
	shift
else
	CURDIR="."
fi
IDENTIFIER="[-_.[:alnum:]][-_.[:alnum:]]*"
NAME='"name"'
CONTEXT='"context"'
VERSION='"version"'
QOSPROFILE='"qosProfile"'
find "$CURDIR" -type f | xargs grep -w -e "${NAME}" -e "${CONTEXT}" -e "${VERSION}" -e "${QOSPROFILE}" |
	grep -v "/.git/" |
	grep -v "/docs/"
