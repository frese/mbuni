#!/usr/bin/bash
# Don't run manually - this is intented to be run by implicitly by make

MBUNI_DEF=/etc/mbuni.def
INSTANCE_NAME=`echo $1|cut -d: -f1`
INSTANCE_LIST=`cat $MBUNI_DEF | grep "^INSTANCES" | cut -d= -f2` 

INSTALL_BIN_DIR=`make echobindir|grep -v make` 

here=`pwd`

if [ x$INSTANCE_NAME = x ]; then
	echo "No instance name given!"
	rm -f $INSTALL_BIN_DIR/*.inst
	exit 1
fi

IFS=,

for i in $INSTANCE_LIST
do
	if [ "x$i" = "x$INSTANCE_NAME" ]; then
		INSTANCE=$i
		break
	fi
done

if [ x$INSTANCE = x ]; then 
	echo "The given instance is not in mbuni.def - aborting..." 
	rm -f $INSTALL_BIN_DIR/*.inst
	exit 1
fi


cd $INSTALL_BIN_DIR

for p in *inst 
do
	if [ -x $p ]; then
		p_first=`echo $p | cut -d. -f1`
		p_extension=`echo $p | cut -d. -f2`
		echo "$p -> $p_first.$INSTANCE_NAME"
		mv $p "$p_first.$INSTANCE_NAME"
	fi
done

cd $here

rm -f instance_installation

exit 0
