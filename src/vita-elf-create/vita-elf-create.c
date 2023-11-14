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
#include "utils/fail-utils.h"
#include "elf-create-argp.h"
#include "utils/yamlemitter.h"

// logging level
int g_log = 0;

#define NONE 0
#define VERBOSE 1
#define DEBUG 2

#define TRACEF(lvl, ...) \
	do { if (g_log >= lvl) printf(__VA_ARGS__); } while (0)

int get_variable_by_symbol(const char *symbol, const vita_elf_t *ve, Elf32_Addr *vaddr);

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		// TRACEF(VERBOSE, "  0x%08X (%s):\n", stubs[i].addr, stubs[i].symbol ? stubs[i].symbol->name : "unreferenced stub");
		// TRACEF(VERBOSE, "    Flags  : 0x%04X\n", stubs[i].library ? stubs[i].library->flags : 0);
		TRACEF(
			VERBOSE,
			"    stub=0x%08X library_nid=0x%08X (%s) target_nid=0x%08X (%s)\n",
			stubs[i].addr,
			stubs[i].library_nid, stubs[i].library ? stubs[i].library->name : "not found",
			stubs[i].target_nid, stubs[i].target ? stubs[i].target->name : "not found"
		);
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

int vita_elf_packing(const char *velf_path, const vita_export_t *exports)
{
	int res;
	char tmp[PATH_MAX];

	int velf_path_length = strnlen(velf_path, PATH_MAX);
	if (velf_path_length >= (PATH_MAX - 4))
		return -1;

	snprintf(tmp, sizeof(tmp), "%s.tmp", velf_path);

	FILE *fd_src, *fd_dst;

	fd_src = fopen(velf_path, "rb");
	if (fd_src == NULL)
		return -1;

	fd_dst = fopen(tmp, "wb");
	if (fd_dst == NULL) {
		res = -1;
		goto end_io_close_src;
	}

	void *elf_header = NULL;

	elf_header = malloc(0x100);
	if (elf_header == NULL) {
		res = -1;
		goto end_io_close_dst;
	}

	/*
	 * Read elf header
	 */
	if (fread(elf_header, 0x100, 1, fd_src) != 1) {
		res = -1;
		goto end_free_elf_header;
	}

	Elf32_Ehdr *pEhdr = (Elf32_Ehdr *)elf_header;

	/*
	 * Remove section entrys
	 */
	pEhdr->e_shoff     = 0;
	pEhdr->e_shentsize = 0;
	pEhdr->e_shnum     = 0;
	pEhdr->e_shstrndx  = 0;

	Elf32_Phdr *pPhdr, *pPhdrTmp;

	pPhdr = (Elf32_Phdr *)(elf_header + pEhdr->e_phoff);

	/*
	 * Packed ehdr and phdr
	 */
	if (pEhdr->e_phoff != pEhdr->e_ehsize) {
		pPhdrTmp = malloc(pEhdr->e_phentsize * pEhdr->e_phnum);

		memcpy(pPhdrTmp, pPhdr, pEhdr->e_phentsize * pEhdr->e_phnum);

		memset(elf_header + pEhdr->e_ehsize, 0, 0x100 - pEhdr->e_ehsize);
		memcpy(elf_header + pEhdr->e_ehsize, pPhdrTmp, pEhdr->e_phentsize * pEhdr->e_phnum);

		free(pPhdrTmp);

		pEhdr->e_phoff = pEhdr->e_ehsize;
		pPhdr = (Elf32_Phdr *)(elf_header + pEhdr->e_phoff);
	}

	long seg_offset = pEhdr->e_ehsize + (pEhdr->e_phentsize * pEhdr->e_phnum);

	/*
	 * Write packed elf header
	 */
	fseek(fd_dst, 0, SEEK_SET);
	if (fwrite(elf_header, seg_offset, 1, fd_dst) != 1) {
		res = -1;
		goto end_free_elf_header;
	}

	void *seg_tmp;

	/*
	 * Write elf segments
	 */
	for (int i=0;i<pEhdr->e_phnum;i++) {

		/*
		 * vita only accepts 0x10 to 0x1000 alignments
		 */
		if (pPhdr[i].p_align > 0x1000) {
			pPhdr[i].p_align = 0x10; // vita elf default align
		}

		seg_offset = (seg_offset + (pPhdr[i].p_align - 1)) & ~(pPhdr[i].p_align - 1);

		if (pPhdr[i].p_filesz != 0) {
			seg_tmp = malloc(pPhdr[i].p_filesz);

			fseek(fd_dst, seg_offset, SEEK_SET);
			fseek(fd_src, pPhdr[i].p_offset, SEEK_SET);

			if (fread(seg_tmp, pPhdr[i].p_filesz, 1, fd_src) != 1) {
				free(seg_tmp);
				res = -1;
				goto end_free_elf_header;
			}

			if (fwrite(seg_tmp, pPhdr[i].p_filesz, 1, fd_dst) != 1) {
				free(seg_tmp);
				res = -1;
				goto end_free_elf_header;
			}

			free(seg_tmp);
			seg_tmp = NULL;
		}

		pPhdr[i].p_offset = seg_offset;
		seg_offset += pPhdr[i].p_filesz;
	}

	seg_offset = pEhdr->e_ehsize + (pEhdr->e_phentsize * pEhdr->e_phnum);

	/*
	 * Write updated elf header
	 */
	fseek(fd_dst, 0, SEEK_SET);
	if (fwrite(elf_header, seg_offset, 1, fd_dst) != 1) {
		res = -1;
		goto end_free_elf_header;
	}

	remove(velf_path);
	rename(tmp, velf_path);

	res = 0;

end_free_elf_header:
	free(elf_header);

end_io_close_dst:
	fclose(fd_dst);

end_io_close_src:
	fclose(fd_src);

	return res;
}


static char* hextostr(int x){
	static char buf[20];
	sprintf(buf,"0x%x",x);
	return buf;
}

static char* booltostr(int x){
	return x ? "true" : "false";
}

static int pack_export_symbols(yaml_emitter_t * emitter, yaml_event_t *event, vita_export_symbol **symbols, size_t symbol_n)
{
	yaml_sequence_start_event_initialize(event, NULL, (unsigned char *)"tag:yaml.org,2002:map", 1, YAML_BLOCK_SEQUENCE_STYLE);
	if (!yaml_emitter_emit(emitter, event))
		return 0;

	for (int i = 0; i < symbol_n; ++i) {
		if (!yamlemitter_mapping_start(emitter, event))
			return 0;

		if(!yamlemitter_key_value(emitter, event, symbols[i]->name, hextostr(symbols[i]->nid)))
			return 0;

		if (!yamlemitter_mapping_end(emitter, event))
			return 0;
	}
	
	yaml_sequence_end_event_initialize(event);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	
	return -1;
	
}

static void write_exports(vita_export_t *exports, const char *export_path)
{
	yaml_emitter_t emitter;
	yaml_event_t event;

	/* Create the Emitter object. */
	yaml_emitter_initialize(&emitter);

	FILE *fp = fopen(export_path, "w");

	if (!fp)
	{
		// TODO: handle this
		fprintf(stderr, "could not open '%s' for writing\n", export_path);
		return;
	}

	yaml_emitter_set_output_file(&emitter, fp);

	/* Create and emit the STREAM-START event. */
	if (!yamlemitter_stream_start(&emitter, &event))
		goto error;

	if (!yamlemitter_document_start(&emitter, &event))
		goto error;

	if (!yamlemitter_mapping_start(&emitter, &event))
		goto error;

	if (!yamlemitter_key(&emitter, &event, exports->name))
		goto error;

	if (!yamlemitter_mapping_start(&emitter, &event))
		goto error;

	if (!yamlemitter_key_value(&emitter, &event, "attributes", hextostr(exports->attributes)))
		goto error;

	if (!yamlemitter_key(&emitter, &event, "version"))
		goto error;

	if (!yamlemitter_mapping_start(&emitter, &event))
		goto error;

	if (!yamlemitter_key_value(&emitter, &event, "major", hextostr(exports->ver_major)))
		goto error;

	if (!yamlemitter_key_value(&emitter, &event, "minor", hextostr(exports->ver_minor)))
		goto error;

	if (!yamlemitter_mapping_end(&emitter, &event))
		goto error;

	if (!yamlemitter_key_value(&emitter, &event, "nid", hextostr(exports->nid)))
		goto error;

	if (exports->start || exports->stop || exports->exit) {
		if (!yamlemitter_key(&emitter, &event, "main"))
			goto error;

		if (!yamlemitter_mapping_start(&emitter, &event))
			goto error;

		if (exports->start) {
			if (!yamlemitter_key_value(&emitter, &event, "start", exports->start))
				goto error;
		}

		if (exports->stop) {
			if (!yamlemitter_key_value(&emitter, &event, "stop", exports->stop))
				goto error;
		}

		if (exports->exit) {
			if (!yamlemitter_key_value(&emitter, &event, "exit", exports->exit))
				goto error;
		}

		if (!yamlemitter_mapping_end(&emitter, &event))
			goto error;
	}

	if (exports->lib_n > 0) {
		if (!yamlemitter_key(&emitter, &event, "libraries"))
			goto error;

		if (!yamlemitter_mapping_start(&emitter, &event))
			goto error;

		for (int i = 0; i < exports->lib_n; ++i) {
			vita_library_export *lib = exports->libs[i];

			if (!yamlemitter_key(&emitter, &event, lib->name))
				goto error;

			if (!yamlemitter_mapping_start(&emitter, &event))
				goto error;

			if (!yamlemitter_key_value(&emitter, &event, "version", hextostr(lib->version)))
				goto error;

			if (!yamlemitter_key_value(&emitter, &event, "syscall", booltostr(lib->syscall)))
				goto error;

			if (!yamlemitter_key_value(&emitter, &event, "nid", hextostr(lib->nid)))
				goto error;

			if (lib->function_n) {
				if (!yamlemitter_key(&emitter, &event, "functions"))
					goto error;

				if (!pack_export_symbols(&emitter, &event, lib->functions, lib->function_n))
					goto error;
			}

			if (lib->variable_n) {
				if (!yamlemitter_key(&emitter, &event, "variables"))
					goto error;

				if (!pack_export_symbols(&emitter, &event, lib->variables, lib->variable_n))
					goto error;
			}

			yaml_mapping_end_event_initialize(&event);
			if (!yaml_emitter_emit(&emitter, &event))
				goto error;
		}

		if (!yamlemitter_mapping_end(&emitter, &event))
			goto error;
	}

	if (!yamlemitter_mapping_end(&emitter, &event))
		goto error;

	if (!yamlemitter_mapping_end(&emitter, &event))
		goto error;

	if (!yamlemitter_document_end(&emitter, &event))
		goto error;

	if (!yamlemitter_stream_end(&emitter, &event))
		goto error;

	/* On error. */
error:
	fclose(fp);
	/* Destroy the Emitter object. */
	yaml_emitter_delete(&emitter);
	// TODO: free exports, free json
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
	fprintf(stderr, "usage: %s [-v|vv|vvv] [-s] [-n] [[-e | -g] config.yml] [-m start,stop,exit] input.elf output.velf\n"
					"\t-v,-vv,-vvv:    logging verbosity (more v is more verbose)\n"
					"\t-s         :    strip the output ELF\n"
					"\t-n         :    allow empty imports\n"
					"\t-e yml     :    optional config options\n"
					"\t-g yml     :    generate an export config from ELF symbols\n"
					"\t-m list    :    specify the list of module entrypoints\n"
					"\tinput.elf  :    input ARM ET_EXEC type ELF\n"
					"\toutput.velf:    output ET_SCE_RELEXEC type ELF\n", argc > 0 ? argv[0] : "vita-elf-create");
	return 0;
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	sce_module_info_t *module_info;
	sce_module_params_t *params;
	sce_section_sizes_t section_sizes;
	void *encoded_modinfo;
	vita_elf_rela_table_t rtable = {};
	vita_export_t *exports = NULL;
	int status = EXIT_SUCCESS;
	int have_libc;

	elf_create_args args = {};
	if (parse_arguments(argc, argv, &args) < 0) {
		usage(argc, argv);
		return EXIT_FAILURE;
	}

	g_log = args.log_level;

	if (args.exports) {
		exports = vita_exports_load(args.exports, args.input, 0);		
		if (!exports)
			return EXIT_FAILURE;

		TRACEF(VERBOSE, "export config loaded from file\n");
	}

	if ((ve = vita_elf_load(args.input, args.check_stub_count, exports)) == NULL)
		return EXIT_FAILURE;

	/* FIXME: save original segment sizes */
	Elf32_Word *segment_sizes = malloc(ve->num_segments * sizeof(Elf32_Word));
	int idx;
	for(idx = 0; idx < ve->num_segments; idx++)
		segment_sizes[idx] = ve->segments[idx].memsz;

	if (!vita_elf_lookup_imports(ve))
		status = EXIT_FAILURE;

	if (!args.exports) {
		// generate a default export list
		exports = vita_export_generate_default(args.input);
		exports->start = args.entrypoint_funcs[0];
		exports->stop = args.entrypoint_funcs[1];
		exports->exit = args.entrypoint_funcs[2];
		if (args.exports_output)
			vita_elf_generate_exports(ve, exports);

		TRACEF(VERBOSE, "export config loaded from default\n");
	}

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

	// Enter module module_sdk_version
	{
		Elf32_Addr module_sdk_version_address = 0xFFFFFFFF;
		if (get_variable_by_symbol("module_sdk_version", ve, &module_sdk_version_address) == 1) {
			const uint32_t *module_sdk_version_ptr = vita_elf_vaddr_to_host(ve, module_sdk_version_address);
			ASSERT(module_sdk_version_ptr != NULL);

			ve->module_sdk_version = *module_sdk_version_ptr;
			ve->module_sdk_version_ptr = module_sdk_version_address;
		}
		else { // failback
			ve->module_sdk_version = PSP2_SDK_VERSION;
			ve->module_sdk_version_ptr = 0xFFFFFFFF;
		}
	}

	have_libc = get_variable_by_symbol("sceLibcHeapSize", ve, NULL)
				|| get_variable_by_symbol("sceLibcHeapExtendedAlloc", ve, NULL)
				|| get_variable_by_symbol("sceLibcHeapDelayedAlloc", ve, NULL)
				|| get_variable_by_symbol("sceLibcHeapInitialSize", ve, NULL)
				|| get_variable_by_symbol("sceLibcHeapUnitSize1MiB", ve, NULL)
				|| get_variable_by_symbol("sceLibcHeapDetectOverrun", ve, NULL);

	if (exports->is_process_image == 0 || exports->is_image_module != 0) {
		have_libc = 0;
	}

	params = sce_elf_module_params_create(ve, exports, have_libc);
	if (!params)
		return EXIT_FAILURE;

	module_info = sce_elf_module_info_create(ve, exports, params->process_param);
	if (!module_info)
		return EXIT_FAILURE;
	
	int total_size = sce_elf_module_info_get_size(module_info, &section_sizes, params, have_libc, ve->vstubs, ve->num_vstubs);
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
			module_info, ve, &section_sizes, &rtable, params);

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

	if (args.exports_output)
		write_exports(exports, args.exports_output);

	if (args.is_test_stripping != 0)
		vita_elf_packing(args.output, exports);

	/* FIXME: restore original segment sizes */
	for(idx = 0; idx < ve->num_segments; idx++)
		ve->segments[idx].memsz = segment_sizes[idx];
	free(segment_sizes);

	sce_elf_module_info_free(module_info);
	sce_elf_module_params_free(params);
	vita_exports_free(exports);
	vita_elf_free(ve);

	return status;
failure:
	return EXIT_FAILURE;
}
