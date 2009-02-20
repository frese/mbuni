# Sets up some Kannel include files and configures MBuni
. ../config.sh

echo "SSLDIR: $SSLDIR"
echo "INSTALLDIR: $INSTALLDIR"

if [ "x$INSTALLDIR" = x ]; then
	echo "INNSTALLDIR is not set. Check config.sh"
	exit
fi

for d in gwlib wap; do
        $SUDO mkdir -p $INSTALLDIR/include/kannel/$d
done

# Kannel install doesn't do this right:
$SUDO cp ../kannel-snapshot/gwlib/*.h $INSTALLDIR/include/kannel/gwlib
$SUDO cp ../kannel-snapshot/gwlib/*.def $INSTALLDIR/include/kannel/gwlib
$SUDO cp ../kannel-snapshot/wap/*.h $INSTALLDIR/include/kannel/wap
$SUDO cp ../kannel-snapshot/wap/*.def $INSTALLDIR/include/kannel/wap

# Configure MBuni:
CFLAGS="-D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_POSIX_PTHREAD_SEMANTICS -L$SSLDIR/lib -DSunOS=1" \
LDFLAGS="-L$INSTALLDIR/lib -L$SSLDIR/lib" \
./configure \
	--prefix=$INSTALLDIR \
	--with-kannel-dir=$INSTALLDIR \
	--with-malloc=native \
        --with-ssl=$SSLDIR \
	--disable-pgsql 

