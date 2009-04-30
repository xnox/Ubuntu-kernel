#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

extern const char *__progname;

int main(int argc, char *argv[])
{
	char *buf, *p;
	struct stat st;
	int fd;
	char *find, *replace, *file;
	int f_size, r_size;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <file> <find> <replace>\n",
			__progname);
		exit(1);
	}

	file = argv[1];
	find = argv[2];
	replace = argv[3];

	f_size = strlen(find) + 1;
	r_size = strlen(replace) + 1;

	if (r_size > f_size) {
		fprintf(stderr, "ERROR: replace strings cannot be longer than find\n");
		exit(1);
	}

	printf("Search for '%s' in '%s', to replace with '%s'...\n",
		find, file, replace);

	if ((fd = open(argv[1], O_RDWR)) == -1) {
		perror(argv[1]);
		exit(1);
	}

	if (fstat(fd, &st) == -1) {
		perror(argv[1]);
		exit(1);
	}

	if ((buf = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED,
			fd, 0)) == MAP_FAILED) {
		perror(argv[1]);
		exit(1);
	}

	for (p = buf; p - buf < st.st_size; p++) {
		/* Speed things up a bit */
		if (*p != *find)
			continue;

		/* Make sure there's enough left */
		if (st.st_size - (p - buf) < f_size)
			break;

		/* We check the terminating nul too */
		if (memcmp(p, find, f_size))
			continue;

		/* Got one, replace it. */
		printf("\tfound occurence at offset %lu...\n", p - buf);

		memset(p, 0, f_size);
		memcpy(p, replace, r_size);

		/* There should be only one, but we keep looking */
	}

	munmap(buf, st.st_size);

	printf("\tCOMPLETED\n");

	exit(0);
}
