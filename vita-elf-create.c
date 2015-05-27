#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		printf("  0x%06x (%s):\n", stubs[i].addr, stubs[i].sym_name ? stubs[i].sym_name : "unreferenced stub");
		printf("    Library: %u\n", stubs[i].library_nid);
		printf("    Module : %u\n", stubs[i].module_nid);
		printf("    NID    : %u\n", stubs[i].target_nid);
	}
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;

	if (argc < 3)
		errx(EXIT_FAILURE,"Usage: vita-elf-create input-elf output-elf");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	if (ve->fstubs_ndx) {
		printf("Function stubs in section %d:\n", ve->fstubs_ndx);
		print_stubs(ve->fstubs, ve->num_fstubs);
	}
	if (ve->vstubs_ndx) {
		printf("Variable stubs in section %d:\n", ve->vstubs_ndx);
		print_stubs(ve->vstubs, ve->num_vstubs);
	}

	vita_elf_free(ve);

	return EXIT_SUCCESS;
}
