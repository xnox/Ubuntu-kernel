
all: source

source: clean
	cd linux*; dpkg-buildpackage -S -sa -rfakeroot -I.git -I.gitignore -i'\.git.*'

version_update:
	cd linux*; dch -i

binary: clean
	cd linux*; debuild -b

clean:
	rm -f *.dsc *.changes *.gz *.deb *.build

