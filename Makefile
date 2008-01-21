
all: source

source:
	cd linux*; dpkg-buildpackage -S -sa -rfakeroot -I.git -I.gitignore -i'\.git.*'

version_update:
	cd linux*; dch -i

clean:
	rm -f *.dsc *.changes *.gz *.deb *.build

