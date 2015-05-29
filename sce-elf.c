#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "elf-defs.h"
#include "elf-utils.h"
#include "sce-elf.h"
#include "fail-utils.h"
#include "varray.h"

const uint32_t sce_elf_stub_func[3] = {
	0xe3e00000,	/* mvn r0, #0 */
	0xe12fff1e,	/* bx lr */
	0xe1a00000	/* mov r0, r0 */
};

#define ALIGN_4(size) (((size) + 3) & ~0x3)

typedef struct {
	uint32_t nid;
	vita_imports_module_t *module;

	union {
		vita_elf_stub_t *functions;
		varray functions_va;
	};

	union {
		vita_elf_stub_t *variables;
		varray variables_va;
	};
} import_module;

static int _stub_sort(const void *el1, const void *el2) {
	const vita_elf_stub_t *stub1 = el1, *stub2 = el2;
	if (stub2->target_nid > stub1->target_nid)
		return 1;
	else if (stub2->target_nid < stub1->target_nid)
		return -1;
	return 0;
}
static int _stub_nid_search(const void *key, const void *element) {
	const uint32_t *nid = key;
	const vita_elf_stub_t *stub = element;
	if (stub->target_nid > *nid)
		return 1;
	else if (stub->target_nid < *nid)
		return -1;
	return 0;
}

static void * _module_init(void *element)
{
	import_module *module = element;
	if (!varray_init(&module->functions_va, sizeof(vita_elf_stub_t), 8)) return NULL;
	if (!varray_init(&module->variables_va, sizeof(vita_elf_stub_t), 4)) return NULL;

	module->functions_va.sort_compar = _stub_sort;
	module->functions_va.search_compar = _stub_nid_search;
	module->variables_va.sort_compar = _stub_sort;
	module->variables_va.search_compar = _stub_nid_search;

	return module;
}
static void _module_destroy(void *element)
{
	import_module *module = element;
	varray_destroy(&module->functions_va);
	varray_destroy(&module->variables_va);
}

static int _module_sort(const void *el1, const void *el2)
{
	const import_module *mod1 = el1, *mod2 = el2;
	if (mod2->nid > mod1->nid)
		return 1;
	else if (mod2->nid < mod1->nid)
		return -1;
	return 0;
}

static int _module_search(const void *key, const void *element)
{
	const uint32_t *nid = key;
	const import_module *module = element;
	if (module->nid > *nid)
		return 1;
	else if (module->nid < *nid)
		return -1;
	return 0;
}

typedef union {
	import_module *modules;
	varray va;
} import_module_list;

static void set_main_module_export(sce_module_exports_t *export, const sce_module_info_t *module_info)
{
	export->size = sizeof(sce_module_exports_raw);
	export->version = 0;
	export->flags = 0x8000;
	export->num_syms_funcs = 1;
	export->num_syms_vars = 1;
	export->nid_table = calloc(2, sizeof(uint32_t));
	export->nid_table[0] = NID_MODULE_START;
	export->nid_table[1] = NID_MODULE_INFO;
	export->entry_table = calloc(2, sizeof(void*));
	export->entry_table[0] = module_info->module_start;
	export->entry_table[1] = module_info;
}

static void set_module_import(vita_elf_t *ve, sce_module_imports_t *import, const import_module *module)
{
	int i;

	import->size = sizeof(sce_module_imports_raw);
	import->version = 1;
	import->num_syms_funcs = module->functions_va.count;
	import->num_syms_vars = module->variables_va.count;
	import->module_nid = module->nid;
	if (module->module) {
		import->module_name = module->module->name;
	}

	import->func_nid_table = calloc(module->functions_va.count, sizeof(uint32_t));
	import->func_entry_table = calloc(module->functions_va.count, sizeof(void *));
	for (i = 0; i < module->functions_va.count; i++) {
		import->func_nid_table[i] = module->functions[i].target_nid;
		import->func_entry_table[i] = vita_elf_vaddr_to_host(ve, module->functions[i].addr);
	}

	import->var_nid_table = calloc(module->variables_va.count, sizeof(uint32_t));
	import->var_entry_table = calloc(module->variables_va.count, sizeof(void *));
	for (i = 0; i < module->variables_va.count; i++) {
		import->var_nid_table[i] = module->variables[i].target_nid;
		import->var_entry_table[i] = vita_elf_vaddr_to_host(ve, module->variables[i].addr);
	}
}

sce_module_info_t *sce_elf_module_info_create(vita_elf_t *ve)
{
	int i;
	sce_module_info_t *module_info;
	import_module_list modlist = {0};
	vita_elf_stub_t *curstub;
	import_module *curmodule;

	module_info = calloc(1, sizeof(sce_module_info_t));
	ASSERT(module_info != NULL);

	module_info->version = 0x0101;
	module_info->module_start = vita_elf_vaddr_to_host(ve, elf32_getehdr(ve->elf)->e_entry);

	module_info->export_top = calloc(1, sizeof(sce_module_exports_t));
	ASSERT(module_info->export_top != NULL);
	module_info->export_end = module_info->export_top + 1;

	set_main_module_export(module_info->export_top, module_info);

	ASSERT(varray_init(&modlist.va, sizeof(import_module), 8));
	modlist.va.init_func = _module_init;
	modlist.va.destroy_func = _module_destroy;
	modlist.va.sort_compar = _module_sort;
	modlist.va.search_compar = _module_search;

	for (i = 0; i < ve->num_fstubs; i++) {
		curstub = ve->fstubs + i;
		curmodule = varray_sorted_search_or_insert(&modlist.va, &curstub->module_nid, NULL);
		ASSERT(curmodule);
		curmodule->nid = curstub->module_nid;
		if (curstub->module)
			curmodule->module = curstub->module;

		varray_sorted_insert_ex(&curmodule->functions_va, curstub, 0);
	}

	for (i = 0; i < ve->num_vstubs; i++) {
		curstub = ve->vstubs + i;
		curmodule = varray_sorted_search_or_insert(&modlist.va, &curstub->module_nid, NULL);
		ASSERT(curmodule);
		curmodule->nid = curstub->module_nid;
		if (curstub->module)
			curmodule->module = curstub->module;

		varray_sorted_insert_ex(&curmodule->variables_va, curstub, 0);
	}

	module_info->import_top = calloc(modlist.va.count, sizeof(sce_module_imports_t));
	ASSERT(module_info->import_top != NULL);
	module_info->import_end = module_info->import_top + modlist.va.count;

	for (i = 0; i < modlist.va.count; i++) {
		set_module_import(ve, module_info->import_top + i, modlist.modules + i);
	}

	return module_info;

failure:
	varray_destroy(&modlist.va);
	sce_elf_module_info_free(module_info);
	return NULL;
}

#define INCR(section, size) do { \
	sizes->section += (size); \
	total_size += (size); \
} while (0)
int sce_elf_module_info_get_size(sce_module_info_t *module_info, sce_section_sizes_t *sizes)
{
	int total_size = 0;
	sce_module_exports_t *export;
	sce_module_imports_t *import;

	memset(sizes, 0, sizeof(*sizes));

	INCR(sceModuleInfo_rodata, sizeof(sce_module_info_raw));
	for (export = module_info->export_top; export < module_info->export_end; export++) {
		INCR(sceLib_ent, sizeof(sce_module_exports_raw));
		if (export->module_name != NULL) {
			INCR(sceExport_rodata, ALIGN_4(strlen(export->module_name) + 1));
		}
		INCR(sceExport_rodata, (export->num_syms_funcs + export->num_syms_vars + export->num_syms_unk) * 8);
	}

	for (import = module_info->import_top; import < module_info->import_end; import++) {
		INCR(sceLib_stubs, sizeof(sce_module_imports_raw));
		if (import->module_name != NULL) {
			INCR(sceImport_rodata, ALIGN_4(strlen(import->module_name) + 1));
		}
		INCR(sceFNID_rodata, import->num_syms_funcs * 4);
		INCR(sceFStub_rodata, import->num_syms_funcs * 4);
		INCR(sceVNID_rodata, import->num_syms_vars * 4);
		INCR(sceVStub_rodata, import->num_syms_vars * 4);
		INCR(sceImport_rodata, import->num_syms_unk * 8);
	}

	return total_size;
}
#undef INCR

void sce_elf_module_info_free(sce_module_info_t *module_info)
{
	sce_module_exports_t *export;
	sce_module_imports_t *import;

	if (module_info == NULL)
		return;

	for (export = module_info->export_top; export < module_info->export_end; export++) {
		free(export->nid_table);
		free(export->entry_table);
	}
	free(module_info->export_top);

	for (import = module_info->import_top; import < module_info->import_end; import++) {
		free(import->func_nid_table);
		free(import->func_entry_table);
		free(import->var_nid_table);
		free(import->var_entry_table);
		free(import->unk_nid_table);
		free(import->unk_entry_table);
	}

	free(module_info->import_top);
	free(module_info);
}

#define INCR(section, size) do { \
	cur_sizes.section += (size); \
	if (cur_sizes.section > sizes->section) \
		FAILX("Attempted to overrun section %s!", #section); \
	section_addrs.section += (size); \
} while (0)
#define ADDR(section) (data + section_addrs.section)
#define INTADDR(section) (*((uint32_t *)ADDR(section)))
#define VADDR(section) (section_addrs.section + segment_base + start_offset)
#define OFFSET(section) (section_addrs.section + start_offset)
#define CONVERT(variable, member, conversion) variable ## _raw->member = conversion(variable->member)
#define CONVERT16(variable, member) CONVERT(variable, member, htole16)
#define CONVERT32(variable, member) CONVERT(variable, member, htole32)
#define CONVERTOFFSET(variable, member) variable ## _raw->member = htole32(vita_elf_host_to_segoffset(ve,variable->member,segndx))
#define SETLOCALPTR(variable, section) do { \
	variable = htole32(VADDR(section)); \
	ADDRELA(&variable); \
} while(0)
#define ADDRELA(localaddr) do { \
	uint32_t addend = le32toh(*((uint32_t *)localaddr)); \
	if (addend) { \
		vita_elf_rela_t *rela = varray_push(&relas, NULL); \
		rela->type = R_ARM_ABS32; \
		rela->offset = ((void*)(localaddr)) - data + segment_base + start_offset; \
		rela->addend = addend; \
	} \
} while(0)
void *sce_elf_module_info_encode(
		const sce_module_info_t *module_info, const vita_elf_t *ve, const sce_section_sizes_t *sizes,
		vita_elf_rela_table_t *rtable)
{
	void *data;
	sce_section_sizes_t cur_sizes = {0};
	sce_section_sizes_t section_addrs = {0};
	int total_size = 0;
	Elf32_Addr segment_base;
	Elf32_Word start_offset;
	int segndx;
	int i;
	sce_module_exports_t *export;
	sce_module_imports_t *import;
	sce_module_info_raw *module_info_raw;
	sce_module_exports_raw *export_raw;
	sce_module_imports_raw *import_raw;
	varray relas;

	ASSERT(varray_init(&relas, sizeof(vita_elf_rela_t), 16));

	for (i = 0; i < sizeof(sce_section_sizes_t) / sizeof(Elf32_Word); i++) {
		((Elf32_Word *)&section_addrs)[i] = total_size;
		total_size += ((Elf32_Word *)sizes)[i];
	}

	segndx = vita_elf_host_to_segndx(ve, module_info->module_start);

	segment_base = ve->segments[segndx].vaddr;
	start_offset = ve->segments[segndx].memsz;

	for (i = 0; i < ve->num_segments; i++) {
		if (i == segndx)
			continue;
		if (ve->segments[i].vaddr >= segment_base + start_offset
				&& ve->segments[i].vaddr < segment_base + start_offset + total_size)
			FAILX("Cannot allocate %d bytes for SCE data at end of segment %d; segment %d overlaps",
					total_size, segndx, i);
	}

	data = calloc(1, total_size);
	ASSERT(data != NULL);

	module_info_raw = (sce_module_info_raw *)ADDR(sceModuleInfo_rodata);
	INCR(sceModuleInfo_rodata, sizeof(sce_module_info_raw));
	CONVERT16(module_info, attributes);
	CONVERT16(module_info, version);
	memcpy(module_info_raw->name, module_info->name, 27);
	module_info_raw->type = module_info->type;
	module_info_raw->export_top = htole32(OFFSET(sceLib_ent));
	module_info_raw->export_end = htole32(OFFSET(sceLib_ent) + sizes->sceLib_ent);
	module_info_raw->import_top = htole32(OFFSET(sceLib_stubs));
	module_info_raw->import_end = htole32(OFFSET(sceLib_stubs) + sizes->sceLib_stubs);
	CONVERT32(module_info, library_nid);
	CONVERT32(module_info, field_38);
	CONVERT32(module_info, field_3C);
	CONVERT32(module_info, field_40);
	CONVERTOFFSET(module_info, module_start);
	CONVERTOFFSET(module_info, module_stop);
	CONVERTOFFSET(module_info, exidx_top);
	CONVERTOFFSET(module_info, exidx_end);
	CONVERTOFFSET(module_info, extab_top);
	CONVERTOFFSET(module_info, extab_end);

	for (export = module_info->export_top; export < module_info->export_end; export++) {
		int num_syms;
		uint32_t *raw_nids, *raw_entries;

		export_raw = (sce_module_exports_raw *)ADDR(sceLib_ent);
		INCR(sceLib_ent, sizeof(sce_module_exports_raw));

		export_raw->size = htole16(sizeof(sce_module_exports_raw));
		CONVERT16(export, version);
		CONVERT16(export, flags);
		CONVERT16(export, num_syms_funcs);
		CONVERT32(export, num_syms_vars);
		CONVERT32(export, num_syms_unk);
		CONVERT32(export, module_nid);
		if (export->module_name != NULL) {
			SETLOCALPTR(export_raw->module_name, sceExport_rodata);
			void *dst = ADDR(sceExport_rodata);
			INCR(sceExport_rodata, ALIGN_4(strlen(export->module_name) + 1));
			strcpy(dst, export->module_name);
		}
		num_syms = export->num_syms_funcs + export->num_syms_vars + export->num_syms_unk;
		SETLOCALPTR(export_raw->nid_table, sceExport_rodata);
		raw_nids = (uint32_t *)ADDR(sceExport_rodata);
		INCR(sceExport_rodata, num_syms * 4);
		SETLOCALPTR(export_raw->entry_table, sceExport_rodata);
		raw_entries = (uint32_t *)ADDR(sceExport_rodata);
		INCR(sceExport_rodata, num_syms * 4);
		for (i = 0; i < num_syms; i++) {
			raw_nids[i] = htole32(export->nid_table[i]);
			if (export->entry_table[i] == module_info) { /* Special case */
				raw_entries[i] = htole32(segment_base + start_offset);
			} else {
				raw_entries[i] = htole32(vita_elf_host_to_vaddr(ve, export->entry_table[i]));
			}
			ADDRELA(raw_entries + i);
		}
	}

	for (import = module_info->import_top; import < module_info->import_end; import++) {
		import_raw = (sce_module_imports_raw *)ADDR(sceLib_stubs);
		INCR(sceLib_stubs, sizeof(sce_module_imports_raw));

		import_raw->size = htole16(sizeof(sce_module_imports_raw));
		CONVERT16(import, version);
		CONVERT16(import, flags);
		CONVERT16(import, num_syms_funcs);
		CONVERT16(import, num_syms_vars);
		CONVERT16(import, num_syms_unk);
		CONVERT32(import, reserved1);
		CONVERT32(import, reserved2);
		CONVERT32(import, module_nid);

		if (import->module_name != NULL) {
			SETLOCALPTR(import_raw->module_name, sceImport_rodata);
			void *dst = ADDR(sceImport_rodata);
			INCR(sceImport_rodata, ALIGN_4(strlen(import->module_name) + 1));
			strcpy(dst, import->module_name);
		}
		if (import->num_syms_funcs) {
			SETLOCALPTR(import_raw->func_nid_table, sceFNID_rodata);
			SETLOCALPTR(import_raw->func_entry_table, sceFStub_rodata);
			for (i = 0; i < import->num_syms_funcs; i++) {
				INTADDR(sceFNID_rodata) = htole32(import->func_nid_table[i]);
				INTADDR(sceFStub_rodata) = htole32(vita_elf_host_to_vaddr(ve, import->func_entry_table[i]));
				ADDRELA(ADDR(sceFStub_rodata));
				INCR(sceFNID_rodata, 4);
				INCR(sceFStub_rodata, 4);
			}
		}
		if (import->num_syms_vars) {
			SETLOCALPTR(import_raw->var_nid_table, sceVNID_rodata);
			SETLOCALPTR(import_raw->var_entry_table, sceVStub_rodata);
			for (i = 0; i < import->num_syms_vars; i++) {
				INTADDR(sceVNID_rodata) = htole32(import->var_nid_table[i]);
				INTADDR(sceVStub_rodata) = htole32(vita_elf_host_to_vaddr(ve, import->var_entry_table[i]));
				ADDRELA(ADDR(sceVStub_rodata));
				INCR(sceVNID_rodata, 4);
				INCR(sceVStub_rodata, 4);
			}
		}
		if (import->num_syms_unk) {
			SETLOCALPTR(import_raw->unk_nid_table, sceImport_rodata);
			for (i = 0; i < import->num_syms_unk; i++) {
				INTADDR(sceImport_rodata) = htole32(import->var_nid_table[i]);
				INCR(sceImport_rodata, 4);
			}
			SETLOCALPTR(import_raw->unk_entry_table, sceImport_rodata);
			for (i = 0; i < import->num_syms_unk; i++) {
				INTADDR(sceImport_rodata) = htole32(vita_elf_host_to_vaddr(ve, import->var_entry_table[i]));
				ADDRELA(ADDR(sceImport_rodata));
				INCR(sceImport_rodata, 4);
			}
		}
	}

	for (i = 0; i < sizeof(sce_section_sizes_t) / sizeof(Elf32_Word); i++) {
		if (((Elf32_Word *)&cur_sizes)[i] != ((Elf32_Word *)sizes)[i]) 
			FAILX("sce_elf_module_info_encode() did not use all space in section %d!", i);
	}

	rtable->num_relas = relas.count;
	rtable->relas = varray_extract_array(&relas);

	return data;
failure:
	free(data);
	return NULL;
}
#undef INCR
#undef ADDR
#undef INTADDR
#undef VADDR
#undef OFFSET
#undef CONVERT
#undef CONVERT16
#undef CONVERT32
#undef CONVERTOFFSET
#undef SETLOCALPTR
#undef ADDRELA

int sce_elf_write_module_info(
		Elf *dest, const vita_elf_t *ve, const sce_section_sizes_t *sizes, void *module_info)
{
	/* Corresponds to the order in sce_section_sizes_t */
	static const char *section_names[] = {
		".sceModuleInfo.rodata",
		".sceLib.ent",
		".sceExport.rodata",
		".sceLib.stubs",
		".sceImport.rodata",
		".sceFNID.rodata",
		".sceFStub.rodata",
		".sceVNID.rodata",
		".sceVStub.rodata"
	};
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	GElf_Phdr phdr;
	Elf_Scn *scn;
	Elf_Data *data;
	sce_section_sizes_t section_addrs = {0};
	int total_size = 0;
	Elf32_Addr segment_base, start_vaddr;
	Elf32_Word start_segoffset, start_foffset;
	int cur_pos;
	int segndx;
	int i;

	for (i = 0; i < sizeof(sce_section_sizes_t) / sizeof(Elf32_Word); i++) {
		((Elf32_Word *)&section_addrs)[i] = total_size;
		total_size += ((Elf32_Word *)sizes)[i];
	}

	ELF_ASSERT(gelf_getehdr(dest, &ehdr));

	for (segndx = 0; segndx < ve->num_segments; segndx++) {
		if (ehdr.e_entry >= ve->segments[segndx].vaddr
				&& ehdr.e_entry < ve->segments[segndx].vaddr + ve->segments[segndx].memsz)
			break;
	}
	ASSERT(segndx < ve->num_segments);

	ELF_ASSERT(gelf_getphdr(dest, segndx, &phdr));

	segment_base = ve->segments[segndx].vaddr;
	start_segoffset = ve->segments[segndx].memsz;

	start_vaddr = segment_base + start_segoffset;
	start_foffset = phdr.p_offset + start_segoffset;
	cur_pos = 0;

	if (!elf_utils_shift_contents(dest, start_foffset, total_size))
		FAILX("Unable to relocate ELF sections");

	phdr.p_filesz += total_size;
	phdr.p_memsz += total_size;
	ELF_ASSERT(gelf_update_phdr(dest, segndx, &phdr));

	ELF_ASSERT(gelf_getehdr(dest, &ehdr));
	ehdr.e_entry = ((segndx & 0x3) << 30) | start_segoffset;
	ELF_ASSERT(gelf_update_ehdr(dest, &ehdr));


	for (i = 0; i < sizeof(sce_section_sizes_t) / sizeof(Elf32_Word); i++) {
		int scn_size = ((Elf32_Word *)sizes)[i];
		if (scn_size == 0)
			continue;

		scn = elf_utils_new_scn_with_name(dest, section_names[i]);
		ELF_ASSERT(gelf_getshdr(scn, &shdr));
		shdr.sh_type = SHT_PROGBITS;
		shdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
		shdr.sh_addr = start_vaddr + cur_pos;
		shdr.sh_offset = start_foffset + cur_pos;
		shdr.sh_size = scn_size;
		shdr.sh_addralign = 4;
		ELF_ASSERT(gelf_update_shdr(scn, &shdr));

		ELF_ASSERT(data = elf_newdata(scn));
		data->d_buf = module_info + cur_pos;
		data->d_type = ELF_T_BYTE;
		data->d_version = EV_CURRENT;
		data->d_size = scn_size;
		data->d_off = 0;
		data->d_align = 1;

		cur_pos += scn_size;
	}

	return 1;
failure:
	return 0;
}

static int sce_rel_short(SCE_Rel *rel, int symseg, int code, int datseg, int offset, int addend)
{
	if (addend > 1 << 11)
		return 0;
	rel->r_short_entry.r_short = 1;
	rel->r_short_entry.r_symseg = symseg;
	rel->r_short_entry.r_code = code;
	rel->r_short_entry.r_datseg = datseg;
	rel->r_short_entry.r_offset_lo = offset & 0xFFF;
	rel->r_short_entry.r_offset_hi = offset >> 20;
	rel->r_short_entry.r_addend = addend;
	return 1;
}

static int sce_rel_long(SCE_Rel *rel, int symseg, int code, int datseg, int offset, int addend)
{
	rel->r_long_entry.r_short = 0;
	rel->r_long_entry.r_symseg = symseg;
	rel->r_long_entry.r_code = code;
	rel->r_long_entry.r_datseg = datseg;
	rel->r_long_entry.r_code2 = 0;
	rel->r_long_entry.r_dist2 = 0;
	rel->r_long_entry.r_offset = offset;
	rel->r_long_entry.r_addend = addend;
	return 1;
}

static int encode_sce_rel(SCE_Rel *rel)
{
	if (rel->r_short_entry.r_short) {
		rel->r_raw_entry.r_word1 = htole32(
				(rel->r_short_entry.r_short) |
				(rel->r_short_entry.r_symseg << 4) |
				(rel->r_short_entry.r_code << 8) |
				(rel->r_short_entry.r_datseg << 16) |
				(rel->r_short_entry.r_offset_lo << 20));
		rel->r_raw_entry.r_word2 = htole32(
				(rel->r_short_entry.r_offset_hi) |
				(rel->r_short_entry.r_addend << 20));

		return 8;
	} else {
		rel->r_raw_entry.r_word1 = htole32(
				(rel->r_long_entry.r_short) |
				(rel->r_long_entry.r_symseg << 4) |
				(rel->r_long_entry.r_code << 8) |
				(rel->r_long_entry.r_datseg << 16) |
				(rel->r_long_entry.r_code2 << 20) |
				(rel->r_long_entry.r_dist2 << 28));
		rel->r_raw_entry.r_word2 = htole32(rel->r_long_entry.r_addend);
		rel->r_raw_entry.r_word3 = htole32(rel->r_long_entry.r_offset);
		return 12;
	}
}

int sce_elf_write_rela_sections(
		Elf *dest, const vita_elf_t *ve, const vita_elf_rela_table_t *rtable)
{
	int total_relas = 0;
	const vita_elf_rela_table_t *curtable;
	const vita_elf_rela_t *vrela;
	void *encoded_relas = NULL, *curpos;
	SCE_Rel rel;
	int relsz;
	int i;
	Elf32_Addr symvaddr;
	Elf32_Word symseg, symoff;
	Elf32_Word datseg, datoff;
	int (*sce_rel_func)(SCE_Rel *, int, int, int, int, int);

	Elf_Scn *scn;
	GElf_Shdr shdr;
	GElf_Phdr *phdrs;
	size_t segment_count = 0;

	for (curtable = rtable; curtable; curtable = curtable->next)
		total_relas += curtable->num_relas;

	ASSERT(encoded_relas = calloc(total_relas, 12));

	sce_rel_func = sce_rel_short;

encode_relas:
	curpos = encoded_relas;

	for (curtable = rtable; curtable; curtable = curtable->next) {
		for (i = 0, vrela = curtable->relas; i < curtable->num_relas; i++, vrela++) {
			datseg = vita_elf_vaddr_to_segndx(ve, vrela->offset);
			datoff = vita_elf_vaddr_to_segoffset(ve, vrela->offset, datseg);
			if (vrela->symbol) {
				symvaddr = vrela->symbol->value + vrela->addend;
			} else {
				symvaddr = vrela->addend;
			}
			symseg = vita_elf_vaddr_to_segndx(ve, symvaddr);
			symoff = vita_elf_vaddr_to_segoffset(ve, symvaddr, symseg);
			if (!sce_rel_func(&rel, symseg, vrela->type, datseg, datoff, symoff)) {
				sce_rel_func = sce_rel_long;
				goto encode_relas;
			}
			relsz = encode_sce_rel(&rel);
			memcpy(curpos, &rel, relsz);
			curpos += relsz;
		}
	}

	scn = elf_utils_new_scn_with_data(dest, ".sce.rel", encoded_relas, curpos - encoded_relas);
	if (scn == NULL)
		goto failure;
	encoded_relas = NULL;

	ELF_ASSERT(gelf_getshdr(scn, &shdr));
	shdr.sh_type = SHT_SCE_RELA;
	shdr.sh_flags = 0;
	shdr.sh_addralign = 4;
	ELF_ASSERT(gelf_update_shdr(scn, &shdr));

	ELF_ASSERT((elf_getphdrnum(dest, &segment_count), segment_count > 0));
	ASSERT(phdrs = calloc(segment_count + 1, sizeof(GElf_Phdr)));
	for (i = 0; i < segment_count; i++) {
		ELF_ASSERT(gelf_getphdr(dest, i, phdrs + i));
	}
	ELF_ASSERT(gelf_newphdr(dest, segment_count + 1));
	ELF_ASSERT(gelf_getphdr(dest, segment_count, phdrs + segment_count));
	phdrs[segment_count].p_type = PT_SCE_RELA;
	phdrs[segment_count].p_offset = shdr.sh_offset;
	phdrs[segment_count].p_filesz = shdr.sh_size;
	phdrs[segment_count].p_align = 16;
	for (i = 0; i < segment_count + 1; i++) {
		ELF_ASSERT(gelf_update_phdr(dest, i, phdrs + i));
	}

	return 1;

failure:
	free(encoded_relas);
	return 0;
}
