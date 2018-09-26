#!/bin/bash

WD=$(dirname $(readlink -e $0))
cd $WD

EXEC='../brubeck'
if [ ! -x "$EXEC" ]; then
	echo "Error: brubeck executible not found, please run build."
	exit 1
fi

TMP=$(mktemp -d)
ROOT=$TMP/brubeck-fiverr
DEB=$ROOT/DEBIAN
ETC=$ROOT/etc
BIN=$ROOT/usr/local/bin

mkdir -p $ETC/init.d $DEB $BIN
cp -v control postinst prerm $DEB
cp -v brubeck-biz.json brubeck-tech.json $ETC
cp -v initd-biz.sh  $ETC/init.d/brubeck-biz
cp -v initd-tech.sh $ETC/init.d/brubeck-tech
cp -v $EXEC $BIN

dpkg-deb --build $ROOT
