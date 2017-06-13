
PROJDIR=$(dirname "$0")
CC="$1"
CFLAGS="$2"
OUTDIR="$3"
LIBS="-lminiupnpc -lncurses"

echo "${CC} ${CFLAGS} ${LIBS} ${PROJDIR}/main.c -o ${OUTDIR}"
$CC $CFLAGS $LIBS $PROJDIR/main.c $PROJDIR/network.c $PROJDIR/upnp.c -o $OUTDIR


echo "$CC $CFLAGS $LIBS $PROJDIR/ncurses-test.c -o $(dirname $OUTDIR)/ncurses"
$CC $CFLAGS $LIBS $PROJDIR/ncurses-test.c -o $(dirname $OUTDIR)/ncurses
