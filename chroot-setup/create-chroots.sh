#!/bin/bash -e

if [ "`id -u`" != "0" ] || [ -n "$FAKEROOTKEY" ]; then
	echo "Must run as root (fakeroot isn't good enough)"
	exit 1
fi

distributions="dapper feisty gutsy hardy intrepid"
arches="i386 amd64"

for dist in $distributions; do
	for arch in $arches; do
		debootstrap --components=main,universe,updates,security \
			--include=`cat other-pkgs/others-$dist` --arch $arch $dist \
			$dist-$arch
		rm -f $dist-$arch/var/cache/apt/archives/*.deb
		(cd $dist-$arch && tar cf ../$dist-$arch.tar .)
		rm -rf $dist-$arch
	done
done
