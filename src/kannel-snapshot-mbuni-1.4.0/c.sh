source ../config.sh

echo "SSLDIR: $SSLDIR"
echo "INSTALLDIR: $INSTALLDIR"

./configure \
	--with-ssl=$SSLDIR \
	--prefix=$INSTALLDIR \
	--with-immutable-realhash \
	--with-malloc=native
