#!/bin/sh

if [ -f skip-svn-check ]; then
	echo "Skipping SVN check"
	exit 0;
fi

if [ `svn status|grep -c -v "?"` -eq 0 ]; then 
	echo "no updates"
	svnversion=`svnversion`
	cat mbuni-config.h | sed s/MBUNI_SVN_VERSION.*/MBUNI_SVN_VERSION\ \"$svnversion\"/g > new-mbuni-config.h
	mv new-mbuni-config.h mbuni-config.h
	exit 0;
fi

echo "Local files are not up to date or has local modifications"

# There are some modified files
exit 1
