#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-import.h"

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		printf("  0x%06x (%s):\n", stubs[i].addr, stubs[i].sym_name ? stubs[i].sym_name : "unreferenced stub");
		printf("    Library: %u (%s)\n", stubs[i].library_nid, stubs[i].library ? stubs[i].library->name : "not found");
		printf("    Module : %u (%s)\n", stubs[i].module_nid, stubs[i].module ? stubs[i].module->name : "not found");
		printf("    NID    : %u (%s)\n", stubs[i].target_nid, stubs[i].target ? stubs[i].target->name : "not found");
	}
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	vita_imports_t *imports;
	int status = EXIT_SUCCESS;

	if (argc < 4)
		errx(EXIT_FAILURE,"Usage: vita-elf-create input-elf output-elf db.json");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	if ((imports = vita_imports_load(argv[3], 0)) == NULL)
		return EXIT_FAILURE;

	if (!vita_elf_lookup_imports(ve, imports))
		status = EXIT_FAILURE;

	if (ve->fstubs_ndx) {
		printf("Function stubs in section %d:\n", ve->fstubs_ndx);
		print_stubs(ve->fstubs, ve->num_fstubs);
	}
	if (ve->vstubs_ndx) {
		printf("Variable stubs in section %d:\n", ve->vstubs_ndx);
		print_stubs(ve->vstubs, ve->num_vstubs);
	}

	vita_elf_free(ve);
	vita_imports_free(imports);

	return status;
}
