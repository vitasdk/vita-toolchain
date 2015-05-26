#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include <libelf.h>

#include "vita-elf.h"

int main(int argc, char *argv[])
{
	vita_elf_t *ve;

	if (argc < 3)
		errx(EXIT_FAILURE,"Usage: vita-elf-create input-elf output-elf");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	fprintf(stderr,"Got fstubs in section %d, vstubs in section %d\n", ve->fstubs_ndx, ve->vstubs_ndx);

	vita_elf_free(ve);

	return EXIT_SUCCESS;
}
