
all: source

source:
	cd linux*; dpkg-buildpackage -S -sa -rfakeroot -I.git -I.gitignore -i'\.git.*'

clean:
	rm -f *.dsc *.changes *.gz

