#!/bin/bash

WD=$(dirname $(readlink -e $0))
cd $WD

EXEC='../brubeck'
if [ ! -x "$EXEC" ]; then
	echo "Error: brubeck executible not found, please run build first."
	exit 1
fi

TMP=$(mktemp -d)
VER=$(awk '/Version:/ {print $2}' control)
ARCH=$(awk '/Architecture:/ {print $2}' control)
ROOT=$TMP/brubeck-fiverr_${VER}_${ARCH}
DEB=$ROOT/DEBIAN
ETC=$ROOT/etc
BIN=$ROOT/usr/local/bin

mkdir -p $ETC/init.d $ETC/brubeck/ $DEB $BIN
cp -v control postinst prerm $DEB
cp -v biz.json tech.json $ETC/brubeck/
cp -v initd-biz.sh  $ETC/init.d/brubeck-biz
cp -v initd-tech.sh $ETC/init.d/brubeck-tech
chmod +x $ETC/init.d/brubeck-biz $ETC/init.d/brubeck-tech
cp -v $EXEC $BIN

dpkg-deb --build $ROOT
