#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <limits.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-import.h"
#include "vita-export.h"
#include "elf-defs.h"
#include "sce-elf.h"
#include "elf-utils.h"
#include "fail-utils.h"
#include "elf-create-argp.h"

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

vita_imports_t **load_imports(elf_create_args *args, int *imports_count)
{
	vita_imports_t **imports = NULL;
	int user_count = args->extra_imports_count;
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
		if ((imports[loaded++] = vita_imports_load(args->extra_imports[i], g_log >= DEBUG)) == NULL)
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

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	vita_imports_t **imports;
	sce_module_info_t *module_info;
	sce_section_sizes_t section_sizes;
	void *encoded_modinfo;
	vita_elf_rela_table_t rtable = {};
	int imports_count;
	vita_export_t *exports = NULL;
	
	int status = EXIT_SUCCESS;

	elf_create_args args = {};
	if (parse_arguments(argc, argv, &args) < 0)
		return EXIT_FAILURE;

	g_log = args.log_level;

	if ((ve = vita_elf_load(args.input, args.check_stub_count)) == NULL)
		return EXIT_FAILURE;

	if (args.exports) {
		exports = vita_exports_load(args.exports, args.input, 0);
		
		if (!exports)
			return EXIT_FAILURE;
	}
	else {
		// generate a default export list
		exports = vita_export_generate_default(args.input);
	}
	
	if (!(imports = load_imports(&args, &imports_count)))
		return EXIT_FAILURE;

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

	encoded_modinfo = sce_elf_module_info_encode(
			module_info, ve, &section_sizes, &rtable);

	TRACEF(VERBOSE, "Relocations from encoded modinfo:\n");
	print_rtable(&rtable);

	FILE *outfile;
	Elf *dest;
	ASSERT(dest = elf_utils_copy_to_file(args.output, ve->elf, &outfile));
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


	sce_elf_module_info_free(module_info);
	vita_elf_free(ve);

	int i;
	for (i = 0; i < imports_count; i++) {
		vita_imports_free(imports[i]);
	}

	free(imports);

	return status;
failure:
	return EXIT_FAILURE;
}
