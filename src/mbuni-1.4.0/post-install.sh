#!/usr/bin/bash
# Don't run manually - this is intented to be run by implicitly by make

echo "Running post install script"

INSTALL_BIN_DIR=`make echobindir|grep -v make`

# Don't do anything if this is an instance installation
[ -f instance_installation ] && exit 0 

here=`pwd`

cd $INSTALL_BIN_DIR

for p in *inst 
do
	if [ -x $p ]; then
		p_first=`echo $p | cut -d. -f1`
		p_extension=`echo $p | cut -d. -f2`
		echo "$p -> $p_first"
		mv $p $p_first
	fi
done

cd $here

exit 0
