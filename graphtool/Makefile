CFLAGS += -Wall

BINDIR=/usr/bin
MANDIR=/usr/share/man/man8

graphtool: graphtool.o
	$(CC) $< -lm -o $@

#graphtool.8.gz: graphtool.8
#	gzip -c $< > $@

clean:
	rm -f graphtool graphtool.o graphtool.8.gz

install: graphtool graphtool.8.gz
	mkdir -p ${DESTDIR}${BINDIR}
	cp graphtool ${DESTDIR}${BINDIR}
	#mkdir -p ${DESTDIR}${MANDIR}
	#cp graphtool.8.gz ${DESTDIR}${MANDIR}
