#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <limits.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-import.h"
#include "elf-defs.h"
#include "sce-elf.h"
#include "elf-utils.h"
#include "fail-utils.h"

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
		TRACEF(VERBOSE, "    Library: %u (%s)\n", stubs[i].library_nid, stubs[i].library ? stubs[i].library->name : "not found");
		TRACEF(VERBOSE, "    Module : %u (%s)\n", stubs[i].module_nid, stubs[i].module ? stubs[i].module->name : "not found");
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
			TRACEF(VERBOSE, "    addr of 8 bytes into segment (%x)\n",
					ve->segments[i].vaddr + 8);
			TRACEF(VERBOSE, "    addr of 16 bytes into segment (%d)\n",
					ve->segments[i].vaddr + 16);
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

void get_binary_directory(char *out, size_t n)
{
	char *c;
	char pathsep = '\0';
#if defined(_WIN32) && !defined(__CYGWIN__)
	GetModuleFileName(NULL, out, n);
	pathsep = '\\';
#elif defined(__linux__) || defined(__CYGWIN__)
	readlink("/proc/self/exe", out, n);
	pathsep = '/';
#elif defined(__APPLE__)
	_NSGetExecutablePath(out, (uint32_t *)&n);
	pathsep = '/';
#elif defined(__FreeBSD__)
	int mib[4];
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;
	sysctl(mib, 4, out, &n, NULL, 0);
	pathsep = '/';
#elif defined(DEFAULT_JSON)
	#error "Sorry, your platform is not supported with -DDEFAULT_JSON."
#endif
	if (pathsep && (c = strrchr(out, pathsep)))
		*++c = '\0';
}

// The format is path1:path2:path3 where all pathes are relative to the binary directory
#ifdef DEFAULT_JSON
char default_json[] = DEFAULT_JSON;
#else
char default_json[] = "";
#endif

vita_imports_t **load_imports(int argc, char *argv[], int *imports_count)
{
	vita_imports_t **imports = NULL;
	int user_count = argc - 3;
	int default_count = 0;
	int loaded = 0;
	char path[PATH_MAX] = { 0 };
	int i;
	char *s;
	char *saveptr;
	int count;
	int base_length;

	for (s = default_json; *s; ++s)
		if (*s == ':')
			++default_count;
	// Only way we get 0 is when default_json is empty
	if (*default_json)
		++default_count;

	count = user_count + default_count;
	imports = calloc(count, sizeof(*imports));
	if (!imports)
		goto failure;

	// First, load default imports
	get_binary_directory(path, sizeof(path));
	base_length = strlen(path);

	s = strtok_r(default_json, ":", &saveptr);
	while (s) {
		strncpy(path + base_length, s, sizeof(path) - base_length - 1);
		if ((imports[loaded++] = vita_imports_load(path, g_log >= DEBUG)) == NULL)
			goto failure;
		s = strtok_r(NULL, ":", &saveptr);
	}

	// Load imports specified by the user
	for (i = 0; i < user_count; i++) {
		if ((imports[loaded++] = vita_imports_load(argv[i + 3], g_log >= DEBUG)) == NULL)
			goto failure;
	}
	*imports_count = count;
	return imports;
failure:
	for (i = 0; i < count; ++i)
		vita_imports_free(imports[i]);
	free(imports);
	return NULL;
}

void free_imports(vita_imports_t **imports, int count)
{
	int i;

	for (i = 0; i < count; i++)
		vita_imports_free(imports[i]);

	free(imports);
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	vita_imports_t **imports;
	sce_module_info_t *module_info;
	sce_section_sizes_t section_sizes;
	void *encoded_modinfo;
	vita_elf_rela_table_t rtable = {};
	int imports_count;

	int status = EXIT_SUCCESS;

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'v') {
		g_log = 1;
		for (int i = 2; argv[1][i] != '\0'; i++) {
			switch (argv[1][i]) {
				case 'v': g_log++; break;
				default: argc = 0; break; // ensure error in next statement
			}
		}
		argv++;
		argc--;
	}

	if (argc < 3)
		errx(EXIT_FAILURE,"Usage: vita-elf-create [-v|-vv] input-elf output-elf [extra.json ...]");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	if (!(imports = load_imports(argc, argv, &imports_count))) {
		vita_elf_free(ve);
		return EXIT_FAILURE;
	}

	if (!vita_elf_lookup_imports(ve, imports, imports_count))
		status = EXIT_FAILURE;

	if (ve->fstubs_ndx) {
		TRACEF(VERBOSE, "Function stubs in section %d:\n", ve->fstubs_ndx);
		print_stubs(ve->fstubs, ve->num_fstubs);
	}
	if (ve->vstubs_ndx) {
		TRACEF(VERBOSE, "Variable stubs in section %d:\n", ve->vstubs_ndx);
		print_stubs(ve->vstubs, ve->num_vstubs);
	}

	TRACEF(VERBOSE, "Relocations:\n");
	list_rels(ve);

	TRACEF(VERBOSE, "Segments:\n");
	list_segments(ve);

	module_info = sce_elf_module_info_create(ve);

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

	strncpy(module_info->name, argv[1], sizeof(module_info->name) - 1);

	encoded_modinfo = sce_elf_module_info_encode(
			module_info, ve, &section_sizes, &rtable);

	TRACEF(VERBOSE, "Relocations from encoded modinfo:\n");
	print_rtable(&rtable);

	FILE *outfile;
	Elf *dest;
	Elf_Scn *sce_rel = NULL;
	int shstrtab_duplicated = 0;
	ASSERT(dest = elf_utils_copy_to_file(argv[2], ve->elf, &outfile));
	ASSERT(elf_utils_duplicate_shstrtab(dest));
	shstrtab_duplicated = 1;
	ASSERT(sce_elf_discard_invalid_relocs(ve, ve->rela_tables));
	ASSERT(sce_elf_write_module_info(dest, ve, &section_sizes, encoded_modinfo));
	rtable.next = ve->rela_tables;
	ASSERT(sce_rel = sce_elf_write_rela_sections(dest, ve, &rtable));
	ASSERT(sce_elf_rewrite_stubs(dest, ve));
	ELF_ASSERT(elf_update(dest, ELF_C_WRITE) >= 0);
	ASSERT(sce_elf_set_headers(outfile, ve));


end:
	if (sce_rel != NULL)
		elf_utils_free_scn_contents(sce_rel);

	if (shstrtab_duplicated)
		elf_utils_free_shstrtab(dest);

	elf_end(dest);

	if (outfile != NULL)
		fclose(outfile);

	free(rtable.relas);
	free(encoded_modinfo);
	sce_elf_module_info_free(module_info);
	vita_elf_free(ve);
	free_imports(imports, imports_count);

	return status;
failure:
	status = EXIT_FAILURE;
	goto end;
}
