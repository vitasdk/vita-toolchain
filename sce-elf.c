#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "vita-elf.h"
#include "elf-defs.h"
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
