#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <limits.h>

#include <libelf.h>
#include <gelf.h>

#include "velf.h"
#include "import.h"
#include "export.h"
#include "elf-defs.h"
#include "sce-elf.h"
#include "elf-utils.h"

// logging level
int g_log = 0;

#define NONE 0
#define VERBOSE 1
#define DEBUG 2

#define TRACEF(lvl, ...) \
	do { if (g_log >= lvl) printf(__VA_ARGS__); } while (0)

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		TRACEF(VERBOSE, "  0x%06x (%s):\n", stubs[i].addr, stubs[i].symbol ? stubs[i].symbol->name : "unreferenced stub");
		TRACEF(VERBOSE, "    Flags  : %u\n", stubs[i].module ? stubs[i].module->flags : 0);
		TRACEF(VERBOSE, "    Library: %u (%s)\n", stubs[i].module_nid, stubs[i].module ? stubs[i].module->name : "not found");
		TRACEF(VERBOSE, "    NID    : %u (%s)\n", stubs[i].target_nid, stubs[i].target ? stubs[i].target->name : "not found");
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

void print_rtable(vita_elf_rela_table_t *rtable)
{
	vita_elf_rela_t *rela;
	int num_relas;

	for (num_relas = rtable->num_relas, rela = rtable->relas; num_relas; num_relas--, rela++) {
		if (rela->symbol) {
			TRACEF(VERBOSE, "    offset %06x: type %s, %s%+d\n",
					rela->offset,
					elf_decode_r_type(rela->type),
					rela->symbol->name, rela->addend);
		} else if (rela->offset) {
			TRACEF(VERBOSE, "    offset %06x: type %s, absolute %06x\n",
					rela->offset,
					elf_decode_r_type(rela->type),
					(uint32_t)rela->addend);
		}
	}
}

void list_rels(vita_elf_t *ve)
{
	vita_elf_rela_table_t *rtable;

	for (rtable = ve->rela_tables; rtable; rtable = rtable->next) {
		TRACEF(VERBOSE, "  Relocations for section %d: %s\n",
				rtable->target_ndx, get_scndx_name(ve, rtable->target_ndx));
		print_rtable(rtable);

	}
}

void list_segments(vita_elf_t *ve)
{
	int i;

	for (i = 0; i < ve->num_segments; i++) {
		TRACEF(VERBOSE, "  Segment %d: vaddr %06x, size 0x%x\n",
				i, ve->segments[i].vaddr, ve->segments[i].memsz);
		if (ve->segments[i].memsz) {
			TRACEF(VERBOSE, "    Host address region: %p - %p\n",
					ve->segments[i].vaddr_top, ve->segments[i].vaddr_bottom);
			TRACEF(VERBOSE, "    4 bytes into segment (%p): %x\n",
					ve->segments[i].vaddr_top + 4, vita_elf_host_to_vaddr(ve, ve->segments[i].vaddr_top + 4));
			TRACEF(VERBOSE, "    addr of 8 bytes into segment (%x): %p\n",
					ve->segments[i].vaddr + 8, vita_elf_vaddr_to_host(ve, ve->segments[i].vaddr + 8));
			TRACEF(VERBOSE, "    12 bytes into segment offset (%p): %d\n",
					ve->segments[i].vaddr_top + 12, vita_elf_host_to_segoffset(ve, ve->segments[i].vaddr_top + 12, i));
			TRACEF(VERBOSE, "    addr of 16 bytes into segment (%d): %p\n",
					16, vita_elf_segoffset_to_host(ve, i, 16));
		}
	}
}

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define strtok_r strtok_s
#elif defined(__linux__) || defined(__CYGWIN__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

static int usage(int argc, char *argv[])
{
	fprintf(stderr, "Usage: %s [-v|vv|vvv] [-n] [-e config.yml] input.elf output.velf\n"
					"\t-v,-vv,-vvv:    logging verbosity (more v is more verbose)\n"
					"\t-n         :    allow empty imports\n"
					"\t-e yml     :    optional config options\n"
					"\tinput.elf  :    input ARM ET_EXEC type ELF\n"
					"\toutput.velf:    output ET_SCE_RELEXEC type ELF\n", argc > 0 ? argv[0] : "vita-elf-create");
	return 0;
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	sce_module_info_t *module_info;
	sce_section_sizes_t section_sizes;
	void *encoded_modinfo;
	vita_elf_rela_table_t rtable = {};
	vita_export_t *exports = NULL;
	char *exports_path = NULL;
	
	int status = EXIT_SUCCESS;

	g_log = 0;
	bool check_stub_count = true;

	int c='?';
	while ((c = getopt(argc, argv, "vne:")) != -1) {
		switch (c) {
		case 'v':g_log++;break;
		case 'e':exports_path = optarg;break;
		case 'n':check_stub_count = false;break;
		default :c='?';break;/**< will be caught later */
		}
	}

	if ((c == '?') || (argc < optind + 2)) {
		usage(argc, argv);
		return EXIT_FAILURE;
	}
	
	char*input = argv[optind];
	char*output = argv[optind+1];
	
	if ((ve = vita_elf_load(input, check_stub_count)) == NULL)
		return EXIT_FAILURE;

	/* FIXME: save original segment sizes */
	Elf32_Word *segment_sizes = malloc(ve->num_segments * sizeof(Elf32_Word));
	int idx;
	for(idx = 0; idx < ve->num_segments; idx++)
		segment_sizes[idx] = ve->segments[idx].memsz;

	if (exports_path) {
		if (!(exports = vita_exports_load(exports_path, input, 0)))
			return EXIT_FAILURE;
	} else {
		// generate a default export list
		exports = vita_export_generate_default(input);
	}

	if (!vita_elf_lookup_imports(ve))
		status = EXIT_FAILURE;

	if (ve->fstubs_va.count) {
		TRACEF(VERBOSE, "Function stubs in sections \n");
		print_stubs(ve->fstubs, ve->num_fstubs);
	}
	if (ve->vstubs_va.count) {
		TRACEF(VERBOSE, "Variable stubs in sections \n");
		print_stubs(ve->vstubs, ve->num_vstubs);
	}

	TRACEF(VERBOSE, "Relocations:\n");
	list_rels(ve);

	TRACEF(VERBOSE, "Segments:\n");
	list_segments(ve);

	module_info = sce_elf_module_info_create(ve, exports);

	if (!module_info)
		return EXIT_FAILURE;
	
	int total_size = sce_elf_module_info_get_size(module_info, &section_sizes);
	int curpos = 0;
	TRACEF(VERBOSE, "Total SCE data size: %d / %x\n", total_size, total_size);
#define PRINTSEC(name) TRACEF(VERBOSE, "  .%.*s.%s: %d (%x @ %x)\n", (int)strcspn(#name,"_"), #name, strchr(#name,'_')+1, section_sizes.name, section_sizes.name, curpos+ve->segments[0].vaddr+ve->segments[0].memsz); curpos += section_sizes.name
	PRINTSEC(sceModuleInfo_rodata);
	PRINTSEC(sceLib_ent);
	PRINTSEC(sceExport_rodata);
	PRINTSEC(sceLib_stubs);
	PRINTSEC(sceImport_rodata);
	PRINTSEC(sceFNID_rodata);
	PRINTSEC(sceFStub_rodata);
	PRINTSEC(sceVNID_rodata);
	PRINTSEC(sceVStub_rodata);

	encoded_modinfo = sce_elf_module_info_encode( module_info, ve, &section_sizes, &rtable);

	TRACEF(VERBOSE, "Relocations from encoded modinfo:\n");
	print_rtable(&rtable);

	FILE *outfile;
	Elf *dest = elf_utils_copy_to_file(output, ve->elf, &outfile);
	ASSERT(dest != NULL, "Unable to copy to file");
	
	ASSERT(elf_utils_duplicate_shstrtab(dest));
	ASSERT(sce_elf_discard_invalid_relocs(ve, ve->rela_tables));
	ASSERT(sce_elf_write_module_info(dest, ve, &section_sizes, encoded_modinfo));
	rtable.next = ve->rela_tables;
	ASSERT(sce_elf_write_rela_sections(dest, ve, &rtable));
	ASSERT(sce_elf_rewrite_stubs(dest, ve));
	ELF_ASSERT(elf_update(dest, ELF_C_WRITE) >= 0);
	elf_end(dest);
	ASSERT(sce_elf_set_headers(outfile, ve));
	fclose(outfile);

	/* FIXME: restore original segment sizes */
	for(idx = 0; idx < ve->num_segments; idx++)
		ve->segments[idx].memsz = segment_sizes[idx];
	free(segment_sizes);

	sce_elf_module_info_free(module_info);
	vita_elf_free(ve);

	return status;
failure:
	return EXIT_FAILURE;
}
