OS="SunOS"
#OS="osx"

if [ $OS = "SunOS" ]; then
	extra_flags="-DSunOS"
        SUDO=""
else
        SUDO="sudo"
fi

INSTALLDIR="/opt/realtime/mbuni"
#INSTALLDIR="/home/realtime/cth/mbuni"

#SSLDIR="/usr"
SSLDIR="/usr/local/ssl"

PATH="$INSTALLDIR/bin:$PATH"

MBUNI_VERSION="1.4.0"

# src directories
K="kannel-snapshot"
M="mbuni"


