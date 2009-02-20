#!/bin/bash

source config.sh

INSTALL=true

if [ $# -gt 0 ]; then
        shift
        while [ x$1 -ne x ]; do
                case $1 in
                        --no-install) INSTALL=false ;;
                        *) echo "unknown argument";;
                esac
        done
fi


# Setup symbolic links
[ -L mbuni ] && rm -f mbuni
[ -L kannel-snapshot ] && rm -f kannel-snapshot

ln -s "mbuni-$MBUNI_VERSION" mbuni
ln -s "kannel-snapshot-mbuni-$MBUNI_VERSION" kannel-snapshot

# K: run kannel and MBuni configuration, compile and install
# M: run MBuni configuration, compile and install
for d in K M; do
	eval "cd \$$(echo $d)"
	source c.sh
	make 
	$SUDO make install 
	cd -
done
