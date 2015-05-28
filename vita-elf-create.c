#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-import.h"
#include "elf-defs.h"

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		printf("  0x%06x (%s):\n", stubs[i].addr, stubs[i].symbol ? stubs[i].symbol->name : "unreferenced stub");
		printf("    Library: %u (%s)\n", stubs[i].library_nid, stubs[i].library ? stubs[i].library->name : "not found");
		printf("    Module : %u (%s)\n", stubs[i].module_nid, stubs[i].module ? stubs[i].module->name : "not found");
		printf("    NID    : %u (%s)\n", stubs[i].target_nid, stubs[i].target ? stubs[i].target->name : "not found");
	}
}

const char *get_scn_name(vita_elf_t *ve, Elf_Scn *scn)
{
	size_t shstrndx;
	GElf_Shdr shdr;

	elf_getshdrstrndx(ve->elf, &shstrndx);
	gelf_getshdr(scn, &shdr);
	return elf_strptr(ve->elf, shstrndx, shdr.sh_name);
}

const char *get_scndx_name(vita_elf_t *ve, int scndx)
{
	return get_scn_name(ve, elf_getscn(ve->elf, scndx));
}

void list_rels(vita_elf_t *ve)
{
	vita_elf_rela_table_t *rtable;
	vita_elf_rela_t *rela;
	int num_relas;

	for (rtable = ve->rela_tables; rtable; rtable = rtable->next) {
		printf("  Relocations for section %d: %s\n",
				rtable->target_ndx, get_scndx_name(ve, rtable->target_ndx));

		for (num_relas = rtable->num_relas, rela = rtable->relas; num_relas; num_relas--, rela++) {
			if (rela->symbol) {
				printf("    offset %06x: type %s, %s%+d\n",
						rela->offset,
						elf_decode_r_type(rela->type),
						rela->symbol->name, rela->addend);
			}
		}
	}
}

void list_segments(vita_elf_t *ve)
{
	int i;

	for (i = 0; i < ve->num_segments; i++) {
		printf("  Segment %d: vaddr %06x, size 0x%x\n",
				i, ve->segments[i].vaddr, ve->segments[i].memsz);
		printf("    Host address region: %p - %p\n",
				ve->segments[i].vaddr_top, ve->segments[i].vaddr_bottom);
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

	printf("Relocations:\n");
	list_rels(ve);

	printf("Segments");
	list_segments(ve);

	vita_elf_free(ve);
	vita_imports_free(imports);

	return status;
}
