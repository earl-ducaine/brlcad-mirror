#!/bin/sh

# Expects file "version" in current directory
# Main result sent to stdout, errors must go to stderr
# Must include the dot, as well as the version number(s)

if test $# != 0 -a $# != 1
then
	echo "Usage: sharedlibvers.sh [directory/libraryname]" 1>&2
	echo "e.g.	../.librt.sun4/librt.so" 1>&2
	exit 1
fi


# Obtain RELEASE, RCS_REVISION, and REL_DATE
if test -r ../gen.sh
then
	eval `grep '^RELEASE=' ../gen.sh`
else
	echo "$0: Unable to get release number"  1>&2
	exit 1
fi

if test $# = 1
then
	DIR=`dirname $1`
else
	DIR=.
fi

cd $DIR

if test -r ./version
then
	MINOR=`cat ./version`
else
	echo "$0: Unable to get minor version number from $DIR/version"  1>&2
	../newvers.sh version_fudge "vers.c file generated by sharedlibvers.sh as fixup."
	MINOR=`cat ./version`
fi

if test $# = 1
then
	echo $1.$RCS_REVISION.$MINOR
	if test ! -f $1.$RCS_REVISION.$MINOR
	then
		echo "$0: ERROR, $1.$RCS_REVISION.$MINOR does not actually exist" 1>&2
	fi
else
	echo .$RCS_REVISION.$MINOR
fi
