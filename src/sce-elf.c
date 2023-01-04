#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-export.h"
#include "elf-defs.h"
#include "elf-utils.h"
#include "sce-elf.h"
#include "fail-utils.h"
#include "varray.h"
#include "endian-utils.h"

const uint32_t sce_elf_stub_func[3] = {
	0xe3e00000,	/* mvn r0, #0 */
	0xe12fff1e,	/* bx lr */
	0xe1a00000	/* mov r0, r0 */
};

#define ALIGN_4(size) (((size) + 3) & ~0x3)

typedef struct {
	uint32_t nid;
	vita_imports_lib_t *library;

	union {
		vita_elf_stub_t *functions;
		varray functions_va;
	};

	union {
		vita_elf_stub_t *variables;
		varray variables_va;
	};
} import_library;

static int _stub_sort(const void *el1, const void *el2) {
	const vita_elf_stub_t *stub1 = el1, *stub2 = el2;
	if (stub1->target_nid > stub2->target_nid)
		return 1;
	else if (stub1->target_nid < stub2->target_nid)
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

static void * _library_init(void *element)
{
	import_library  *library = element;
	if (!varray_init(&library->functions_va, sizeof(vita_elf_stub_t), 8)) return NULL;
	if (!varray_init(&library->variables_va, sizeof(vita_elf_stub_t), 4)) return NULL;

	library->functions_va.sort_compar = _stub_sort;
	library->functions_va.search_compar = _stub_nid_search;
	library->variables_va.sort_compar = _stub_sort;
	library->variables_va.search_compar = _stub_nid_search;

	return library;
}
static void _library_destroy(void *element)
{
	import_library  *library = element;
	varray_destroy(&library->functions_va);
	varray_destroy(&library->variables_va);
}

static int _library_sort(const void *el1, const void *el2)
{
	const import_library  *lib1 = el1, *lib2 = el2;
	if (lib2->nid > lib1->nid)
		return 1;
	else if (lib2->nid < lib1->nid)
		return -1;
	return 0;
}

static int _library_search(const void *key, const void *element)
{
	const uint32_t *nid = key;
	const import_library  *library = element;
	if (library->nid > *nid)
		return 1;
	else if (library->nid < *nid)
		return -1;
	return 0;
}

static int get_function_by_symbol(const char *symbol, const vita_elf_t *ve, Elf32_Addr *vaddr) {
	int i;
	
	for (i = 0; i < ve->num_symbols; ++i) {
		if (ve->symtab[i].type != STT_FUNC)
			continue;
		
		if (strcmp(ve->symtab[i].name, symbol) == 0) {
			if (vaddr) {
				*vaddr = ve->symtab[i].value;
			}
			break;
		}
	}
	
	return i != ve->num_symbols;
}

int get_variable_by_symbol(const char *symbol, const vita_elf_t *ve, Elf32_Addr *vaddr) {
	int i;
	
	for (i = 0; i < ve->num_symbols; ++i) {
		if (ve->symtab[i].type != STT_OBJECT)
			continue;
		
		if (strcmp(ve->symtab[i].name, symbol) == 0) {
			if (vaddr) {
				*vaddr = ve->symtab[i].value;
			}
			break;
		}
	}
	
	return i != ve->num_symbols;
}

typedef union {
	import_library  *libs;
	varray va;
} import_library_list;

static int set_module_export(vita_elf_t *ve, sce_module_exports_t *export, vita_library_export *lib)
{
	export->size = sizeof(sce_module_exports_raw);
	export->version = lib->version;
	export->flags = lib->syscall ? 0x4001 : 0x0001;
	export->num_syms_funcs = lib->function_n;
	export->num_syms_vars = lib->variable_n;
	export->library_name = strdup(lib->name);
	export->library_nid = lib->nid; 
	
	int total_exports = export->num_syms_funcs + export->num_syms_vars;
	export->nid_table = calloc(total_exports, sizeof(uint32_t));
	export->entry_table = calloc(total_exports, sizeof(void*));
	
	int cur_ent = 0;
	int i;
	for (i = 0; i < export->num_syms_funcs; ++i) {
		Elf32_Addr vaddr = 0;
		vita_export_symbol *sym = lib->functions[i];
		
		if (!get_function_by_symbol(sym->name, ve, &vaddr)) {
			FAILX("Could not find function symbol '%s' for export '%s'", sym->name, lib->name);
		}
		
		export->nid_table[cur_ent] = sym->nid;
		export->entry_table[cur_ent] = vita_elf_vaddr_to_host(ve, vaddr);
		++cur_ent;
	}
	
	for (i = 0; i < export->num_syms_vars; ++i) {
		Elf32_Addr vaddr = 0;
		vita_export_symbol *sym = lib->variables[i];
		
		if (!get_variable_by_symbol(sym->name, ve, &vaddr)) {
			FAILX("Could not find variable symbol '%s' for export '%s'", sym->name, lib->name);
		}
		
		export->nid_table[cur_ent] = sym->nid;
		export->entry_table[cur_ent] = vita_elf_vaddr_to_host(ve, vaddr);
		++cur_ent;
	}
	
	return 0;
	
failure:
	return -1;
}

static int set_main_module_export(vita_elf_t *ve, sce_module_exports_t *export, sce_module_info_t *module_info, vita_export_t *export_spec, void *process_param)
{
	export->size = sizeof(sce_module_exports_raw);
	export->version = 0;
	export->flags = 0x8000; // for syslib
	export->num_syms_funcs = (export_spec->is_image_module == 0) ? 1 : 0; // Really all modules have module_start?
	export->num_syms_vars = 2; // module_info/process_param for default

	if (export_spec->is_image_module == 0) {
		if (export_spec->bootstart)
			++export->num_syms_funcs;

		if (export_spec->stop)
			++export->num_syms_funcs;

		if (export_spec->exit)
			++export->num_syms_funcs;
	}

	if (ve->module_sdk_version_ptr != 0xFFFFFFFF) {
		++export->num_syms_vars;
	}

	int total_exports = export->num_syms_funcs + export->num_syms_vars;
	export->nid_table = calloc(total_exports, sizeof(uint32_t));
	export->entry_table = calloc(total_exports, sizeof(void*));
	
	int cur_nid = 0;
	if (export_spec->is_image_module == 0) {
		if (export_spec->start) {
			Elf32_Addr vaddr = 0;
			if (!get_function_by_symbol(export_spec->start, ve, &vaddr)) {
				FAILX("Could not find symbol '%s' for main export 'start'", export_spec->start);
			}

			module_info->module_start = vita_elf_vaddr_to_host(ve, vaddr);
		} else {
			module_info->module_start = vita_elf_vaddr_to_host(ve, elf32_getehdr(ve->elf)->e_entry);
		}

		export->nid_table[cur_nid] = NID_MODULE_START;
		export->entry_table[cur_nid] = module_info->module_start;
		++cur_nid;

		if (export_spec->bootstart) {
			Elf32_Addr vaddr = 0;

			if (!get_function_by_symbol(export_spec->bootstart, ve, &vaddr)) {
				FAILX("Could not find symbol '%s' for main export 'bootstart'", export_spec->bootstart);
			}
		
			export->nid_table[cur_nid] = NID_MODULE_BOOTSTART;
			export->entry_table[cur_nid] = vita_elf_vaddr_to_host(ve, vaddr);
			++cur_nid;
		}

		if (export_spec->stop) {
			Elf32_Addr vaddr = 0;

			if (!get_function_by_symbol(export_spec->stop, ve, &vaddr)) {
				FAILX("Could not find symbol '%s' for main export 'stop'", export_spec->stop);
			}

			export->nid_table[cur_nid] = NID_MODULE_STOP;
			export->entry_table[cur_nid] = module_info->module_stop = vita_elf_vaddr_to_host(ve, vaddr);
			++cur_nid;
		}

		if (export_spec->exit) {
			Elf32_Addr vaddr = 0;

			if (!get_function_by_symbol(export_spec->exit, ve, &vaddr)) {
				FAILX("Could not find symbol '%s' for main export 'exit'", export_spec->exit);
			}

			export->nid_table[cur_nid] = NID_MODULE_EXIT;
			export->entry_table[cur_nid] = vita_elf_vaddr_to_host(ve, vaddr);
			++cur_nid;
		}
	}

	export->nid_table[cur_nid] = NID_MODULE_INFO;
	export->entry_table[cur_nid] = module_info;
	++cur_nid;
	
	export->nid_table[cur_nid] = NID_PROCESS_PARAM;
	export->entry_table[cur_nid] = process_param;
	++cur_nid;

	if (ve->module_sdk_version_ptr != 0xFFFFFFFF) {
		export->nid_table[cur_nid] = NID_MODULE_SDK_VERSION;
		export->entry_table[cur_nid] = vita_elf_vaddr_to_host(ve, ve->module_sdk_version_ptr);
		++cur_nid;
	}

	return 0;
	
failure:
	return -1;
}

static void set_module_import(vita_elf_t *ve, sce_module_imports_t *import, const import_library *library)
{
	int i;

	import->size = sizeof(sce_module_imports_raw);

	if (((library->library->flags >> 16) & 0xFFFF) <= 1) {
		import->version = 1;
	} else {
		import->version = ((library->library->flags >> 16) & 0xFFFF);
	}
	import->num_syms_funcs = library->functions_va.count;
	import->num_syms_vars = library->variables_va.count;
	import->library_nid = library->nid;
	import->flags = library->library->flags & 0xFFFF;

	if (library->library) {
		import->library_name = library->library->name;
	}

	import->func_nid_table = calloc(library->functions_va.count, sizeof(uint32_t));
	import->func_entry_table = calloc(library->functions_va.count, sizeof(void *));
	for (i = 0; i < library->functions_va.count; i++) {
		import->func_nid_table[i] = library->functions[i].target_nid;
		import->func_entry_table[i] = vita_elf_vaddr_to_host(ve, library->functions[i].addr);
	}

	import->var_nid_table = calloc(library->variables_va.count, sizeof(uint32_t));
	import->var_entry_table = calloc(library->variables_va.count, sizeof(void *));
	for (i = 0; i < library->variables_va.count; i++) {
		import->var_nid_table[i] = library->variables[i].target_nid;
		import->var_entry_table[i] = vita_elf_vaddr_to_host(ve, library->variables[i].addr);
	}
}

sce_module_params_t *sce_elf_module_params_create(vita_elf_t *ve, int have_libc) 
{
	sce_module_params_t *params = calloc(1, sizeof(sce_module_params_t));
	ASSERT(params != NULL);

	params->process_param_version = ve->module_sdk_version;

	if (params->process_param_version >= VITA_TOOLCHAIN_PROCESS_PARAM_NEW_FORMAT_VERSION) {
		params->process_param_size = sizeof(sce_process_param_v6_raw);

		sce_process_param_v6_raw *process_param;

		process_param = calloc(1, sizeof(*process_param));
		ASSERT(sizeof(*process_param) == 0x34);
		ASSERT(process_param != NULL);
		process_param->size = 0x34;
		memcpy(&(process_param->magic), "PSP2", 4);
		process_param->version = 6;
		process_param->fw_version = params->process_param_version;

		params->process_param = process_param;
	} else {
		params->process_param_size = sizeof(sce_process_param_v5_raw);

		sce_process_param_v5_raw *process_param;

		process_param = calloc(1, sizeof(*process_param));
		ASSERT(sizeof(*process_param) == 0x30);
		ASSERT(process_param != NULL);
		process_param->size = 0x30;
		memcpy(&(process_param->magic), "PSP2", 4);
		process_param->version = 5;
		process_param->fw_version = params->process_param_version;

		params->process_param = process_param;
	}

	if (have_libc) {
		params->libc_param = calloc(1, sizeof(sce_libc_param_t));
		ASSERT(params->libc_param != NULL);
		params->libc_param->size = 0x38;
		params->libc_param->unk_0x1C = 9;
		params->libc_param->fw_version = ve->module_sdk_version;
		params->libc_param->_default_heap_size = 0x40000;
	}

	return params;

failure:
	if (params->process_param != NULL)
		free(params->process_param);

	if (params->libc_param != NULL)
		free(params->libc_param);

	if (params != NULL)
		free(params);

	return NULL;
}

void sce_elf_module_params_free(sce_module_params_t *params)
{
	if (params == NULL)
		return;

	if (params->libc_param)
		free(params->libc_param);
	free(params->process_param);
	free(params);
}

sce_module_info_t *sce_elf_module_info_create(vita_elf_t *ve, vita_export_t *exports, void* process_param)
{
	int i;
	sce_module_info_t *module_info;
	import_library_list liblist = {0};
	vita_elf_stub_t *curstub;
	import_library  *curlib;

	module_info = calloc(1, sizeof(sce_module_info_t));
	ASSERT(module_info != NULL);

	module_info->type = 6;
	module_info->version = (exports->ver_major << 8) | exports->ver_minor;
	
	strncpy(module_info->name, exports->name, sizeof(module_info->name) - 1);
	
	// allocate memory for all libraries + main
	module_info->export_top = calloc(exports->lib_n + 1, sizeof(sce_module_exports_t));
	ASSERT(module_info->export_top != NULL);
	module_info->export_end = module_info->export_top + exports->lib_n + 1;

	if (set_main_module_export(ve, module_info->export_top, module_info, exports, process_param) < 0) {
		goto sce_failure;
	}

	// populate rest of exports
	for (i = 0; i < exports->lib_n; ++i) {
		vita_library_export *lib = exports->libs[i];
		sce_module_exports_t *exp = (sce_module_exports_t *)(module_info->export_top + i + 1);
		
		// TODO: improve cleanup
		if (set_module_export(ve, exp, lib) < 0) {
			goto sce_failure;
		}
	}
	
	ASSERT(varray_init(&liblist.va, sizeof(import_library ), 8));
	liblist.va.init_func = _library_init;
	liblist.va.destroy_func = _library_destroy;
	liblist.va.sort_compar = _library_sort;
	liblist.va.search_compar = _library_search;

	for (i = 0; i < ve->num_fstubs; i++) {
		curstub = ve->fstubs + i;
		curlib = varray_sorted_search_or_insert(&liblist.va, &curstub->library_nid, NULL);
		ASSERT(curlib);
		curlib->nid = curstub->library_nid;
		if (curstub->library)
			curlib->library = curstub->library;

		varray_sorted_insert_ex(&curlib->functions_va, curstub, 0);
	}

	for (i = 0; i < ve->num_vstubs; i++) {
		curstub = ve->vstubs + i;
		curlib = varray_sorted_search_or_insert(&liblist.va, &curstub->library_nid, NULL);
		ASSERT(curlib);
		curlib->nid = curstub->library_nid;
		if (curstub->library)
			curlib->library = curstub->library;

		varray_sorted_insert_ex(&curlib->variables_va, curstub, 0);
	}

	module_info->import_top = calloc(liblist.va.count, sizeof(sce_module_imports_t));
	ASSERT(module_info->import_top != NULL);
	module_info->import_end = module_info->import_top + liblist.va.count;

	for (i = 0; i < liblist.va.count; i++) {
		set_module_import(ve, module_info->import_top + i, liblist.libs + i);
	}

	// fake param. Required for old fw.
	if (ve->module_sdk_version < VITA_TOOLCHAIN_PROCESS_PARAM_NEW_FORMAT_VERSION) {
		module_info->exidx_top = ve->segments[0].vaddr_top + 0;
		module_info->exidx_end = ve->segments[0].vaddr_top + 1;
	}

	return module_info;

failure:
	varray_destroy(&liblist.va);
	
sce_failure:
	sce_elf_module_info_free(module_info);
	return NULL;
}

#define INCR(section, size) do { \
	sizes->section += (size); \
	total_size += (size); \
} while (0)
int sce_elf_module_info_get_size(sce_module_info_t *module_info, sce_section_sizes_t *sizes, sce_module_params_t *params, int have_libc, vita_elf_stub_t *vstubs, uint32_t num_vstubs)
{
	int total_size = 0;
	sce_module_exports_t *export;
	sce_module_imports_t *import;

	memset(sizes, 0, sizeof(*sizes));

	INCR(sceModuleInfo_rodata, sizeof(sce_module_info_raw));
	INCR(sceModuleInfo_rodata, params->process_param_size);
	if (have_libc)
		INCR(sceModuleInfo_rodata, sizeof(sce_libc_param_raw));

	for (export = module_info->export_top; export < module_info->export_end; export++) {
		INCR(sceLib_ent, sizeof(sce_module_exports_raw));
		if (export->library_name != NULL) {
			INCR(sceExport_rodata, ALIGN_4(strlen(export->library_name) + 1));
		}
		INCR(sceExport_rodata, (export->num_syms_funcs + export->num_syms_vars + export->num_syms_tls_vars) * 8);
	}

	for (import = module_info->import_top; import < module_info->import_end; import++) {
		INCR(sceLib_stubs, sizeof(sce_module_imports_raw));
		if (import->library_name != NULL) {
			INCR(sceImport_rodata, ALIGN_4(strlen(import->library_name) + 1));
		}
		INCR(sceFNID_rodata, import->num_syms_funcs * 4);
		INCR(sceFStub_rodata, import->num_syms_funcs * 4);
		INCR(sceVNID_rodata, import->num_syms_vars * 4);
		INCR(sceVStub_rodata, import->num_syms_vars * 4);
		INCR(sceImport_rodata, import->num_syms_tls_vars * 8);
	}

	for (int i = 0; i < num_vstubs; i++)
		INCR(sceVStub_rodata, sizeof(Elf32_Word) + vstubs[i].rel_info_size);

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
		free(import->tls_var_nid_table);
		free(import->tls_var_entry_table);
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
		vita_elf_rela_table_t *rtable, sce_module_params_t *params)
{
	void *data;
	sce_section_sizes_t cur_sizes = {0};
	sce_section_sizes_t section_addrs = {0};
	int total_size = 0;
	Elf32_Addr segment_base;
	Elf32_Word start_offset;
	uint32_t process_param_offset = sizeof(sce_module_info_raw);
	uint32_t libc_param_offset = process_param_offset + params->process_param_size;
	int segndx;
	int i, j;
	sce_module_exports_t *export;
	sce_module_imports_t *import;
	sce_module_info_raw *module_info_raw;
	sce_libc_param_t *libc_param = params->libc_param;
	sce_libc_param_raw *libc_param_raw;
	sce_module_exports_raw *export_raw;
	sce_module_imports_raw *import_raw;
	varray relas;
	Elf32_Word *vstub_addr;
	Elf32_Addr vstub_vaddr;
	SCE_Rel *vstub_rel;
	uint32_t vstubs_size;
	vita_elf_stub_t *import_vstub;

	ASSERT(varray_init(&relas, sizeof(vita_elf_rela_t), 16));

	for (i = 0; i < sizeof(sce_section_sizes_t) / sizeof(Elf32_Word); i++) {
		((Elf32_Word *)&section_addrs)[i] = total_size;
		total_size += ((Elf32_Word *)sizes)[i];
	}

	segndx = (module_info->module_start != NULL) ? vita_elf_host_to_segndx(ve, module_info->module_start) : 0;

	segment_base = ve->segments[segndx].vaddr;
	start_offset = ve->segments[segndx].memsz;
	start_offset = (start_offset + 0xF) & ~0xF; // align to 16 bytes

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

	if (libc_param != NULL) {
		libc_param_raw = (sce_libc_param_raw *)(ADDR(sceModuleInfo_rodata) + libc_param_offset);
		CONVERT32(libc_param, size);
		CONVERT32(libc_param, fw_version);
		CONVERT32(libc_param, unk_0x1C);
		CONVERT32(libc_param, _default_heap_size);
		libc_param_raw->default_heap_size = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, _default_heap_size);
		get_variable_by_symbol("sceLibcHeapUnitSize1MiB", ve, &libc_param_raw->heap_unit_1mb);
		get_variable_by_symbol("sceLibcHeapSize", ve, &libc_param_raw->heap_size);
		get_variable_by_symbol("sceLibcHeapInitialSize", ve, &libc_param_raw->heap_initial_size);
		get_variable_by_symbol("sceLibcHeapExtendedAlloc", ve, &libc_param_raw->heap_extended_alloc);
		get_variable_by_symbol("sceLibcHeapDelayedAlloc", ve, &libc_param_raw->heap_delayed_alloc);
		get_variable_by_symbol("sceLibcHeapDetectOverrun", ve, &libc_param_raw->heap_detect_overrun);
		libc_param_raw->malloc_replace = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, _malloc_replace);
		libc_param_raw->new_replace = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, _new_replace);
		libc_param_raw->malloc_for_tls_replace = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, _malloc_for_tls_replace);
		ADDRELA(&libc_param_raw->heap_unit_1mb);
		ADDRELA(&libc_param_raw->default_heap_size);
		ADDRELA(&libc_param_raw->heap_size);
		ADDRELA(&libc_param_raw->heap_initial_size);
		ADDRELA(&libc_param_raw->heap_extended_alloc);
		ADDRELA(&libc_param_raw->heap_delayed_alloc);
		ADDRELA(&libc_param_raw->heap_detect_overrun);
		ADDRELA(&libc_param_raw->malloc_replace);
		ADDRELA(&libc_param_raw->new_replace);
		ADDRELA(&libc_param_raw->malloc_for_tls_replace);
		{
			libc_param_raw->_malloc_replace.size = 0x34;
			libc_param_raw->_malloc_replace.unk_0x4 = 1;
			libc_param_raw->_new_replace.size = 0x28;
			libc_param_raw->_new_replace.unk_0x4 = 1;
			libc_param_raw->_malloc_for_tls_replace.size = 0x18;
			libc_param_raw->_malloc_for_tls_replace.unk_0x4 = 0x1;

			// Memory allocation function names were found here: https://github.com/Olde-Skuul/burgerlib/blob/master/source/vita/brvitamemory.h
			// Credit to Rebecca Ann Heineman <becky@burgerbecky.com>
			get_function_by_symbol("user_malloc_init", ve, &libc_param_raw->_malloc_replace.malloc_init);
			get_function_by_symbol("user_malloc_finalize", ve, &libc_param_raw->_malloc_replace.malloc_term);
			get_function_by_symbol("user_malloc", ve, &libc_param_raw->_malloc_replace.malloc);
			get_function_by_symbol("user_free", ve, &libc_param_raw->_malloc_replace.free);
			get_function_by_symbol("user_calloc", ve, &libc_param_raw->_malloc_replace.calloc);
			get_function_by_symbol("user_realloc", ve, &libc_param_raw->_malloc_replace.realloc);
			get_function_by_symbol("user_memalign", ve, &libc_param_raw->_malloc_replace.memalign);
			get_function_by_symbol("user_reallocalign", ve, &libc_param_raw->_malloc_replace.reallocalign);
			get_function_by_symbol("user_malloc_stats", ve, &libc_param_raw->_malloc_replace.malloc_stats);
			get_function_by_symbol("user_malloc_stats_fast", ve, &libc_param_raw->_malloc_replace.malloc_stats_fast);
			get_function_by_symbol("user_malloc_usable_size", ve, &libc_param_raw->_malloc_replace.malloc_usable_size);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc_init);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc_term);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc);
			ADDRELA(&libc_param_raw->_malloc_replace.free);
			ADDRELA(&libc_param_raw->_malloc_replace.calloc);
			ADDRELA(&libc_param_raw->_malloc_replace.realloc);
			ADDRELA(&libc_param_raw->_malloc_replace.memalign);
			ADDRELA(&libc_param_raw->_malloc_replace.reallocalign);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc_stats);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc_stats_fast);
			ADDRELA(&libc_param_raw->_malloc_replace.malloc_usable_size);

			get_function_by_symbol("user_malloc_for_tls_init", ve, &libc_param_raw->_malloc_for_tls_replace.malloc_init_for_tls);
			get_function_by_symbol("user_malloc_for_tls_finalize", ve, &libc_param_raw->_malloc_for_tls_replace.malloc_term_for_tls);
			get_function_by_symbol("user_malloc_for_tls", ve, &libc_param_raw->_malloc_for_tls_replace.malloc_for_tls);
			get_function_by_symbol("user_free_for_tls", ve, &libc_param_raw->_malloc_for_tls_replace.free_for_tls);
			ADDRELA(&libc_param_raw->_malloc_for_tls_replace.malloc_init_for_tls);
			ADDRELA(&libc_param_raw->_malloc_for_tls_replace.malloc_term_for_tls);
			ADDRELA(&libc_param_raw->_malloc_for_tls_replace.malloc_for_tls);
			ADDRELA(&libc_param_raw->_malloc_for_tls_replace.free_for_tls);

			// user_new(std::size_t) throw(std::badalloc)
			get_function_by_symbol("_Z8user_newj", ve, &libc_param_raw->_new_replace.operator_new);
			// user_new(std::size_t, std::nothrow_t const&)
			get_function_by_symbol("_Z8user_newjRKSt9nothrow_t", ve, &libc_param_raw->_new_replace.operator_new_nothrow);
			// user_new_array(std::size_t) throw(std::badalloc)
			get_function_by_symbol("_Z14user_new_arrayj", ve, &libc_param_raw->_new_replace.operator_new_arr);
			// user_new_array(std::size_t, std::nothrow_t const&)
			get_function_by_symbol("_Z14user_new_arrayjRKSt9nothrow_t", ve, &libc_param_raw->_new_replace.operator_new_arr_nothrow);
			// user_delete(void*)
			get_function_by_symbol("_Z11user_deletePv", ve, &libc_param_raw->_new_replace.operator_delete);
			// user_delete(void*, std::nothrow_t const&)
			get_function_by_symbol("_Z11user_deletePvRKSt9nothrow_t", ve, &libc_param_raw->_new_replace.operator_delete_nothrow);
			// user_delete_array(void*)
			get_function_by_symbol("_Z17user_delete_arrayPv", ve, &libc_param_raw->_new_replace.operator_delete_arr);
			// user_delete_array(void*, std::nothrow_t const&)
			get_function_by_symbol("_Z17user_delete_arrayPvRKSt9nothrow_t", ve, &libc_param_raw->_new_replace.operator_delete_arr_nothrow);
			ADDRELA(&libc_param_raw->_new_replace.operator_new);
			ADDRELA(&libc_param_raw->_new_replace.operator_new_nothrow);
			ADDRELA(&libc_param_raw->_new_replace.operator_new_arr);
			ADDRELA(&libc_param_raw->_new_replace.operator_new_arr_nothrow);
			ADDRELA(&libc_param_raw->_new_replace.operator_delete);
			ADDRELA(&libc_param_raw->_new_replace.operator_delete_nothrow);
			ADDRELA(&libc_param_raw->_new_replace.operator_delete_arr);
			ADDRELA(&libc_param_raw->_new_replace.operator_delete_arr_nothrow);
		}
	}

	if (params->process_param_size == sizeof(sce_process_param_v6_raw)) {

		sce_process_param_v6_t *process_param;
		sce_process_param_v6_raw *process_param_raw;

		process_param = (sce_process_param_v6_t *)(params->process_param);
		process_param_raw = (sce_process_param_v6_raw *)(ADDR(sceModuleInfo_rodata) + process_param_offset);

		CONVERT32(process_param, size);
		CONVERT32(process_param, magic);
		CONVERT32(process_param, version);
		CONVERT32(process_param, fw_version);
		get_variable_by_symbol("sceUserMainThreadName", ve, &process_param_raw->main_thread_name);
		get_variable_by_symbol("sceUserMainThreadPriority", ve, &process_param_raw->main_thread_priority);
		get_variable_by_symbol("sceUserMainThreadStackSize", ve, &process_param_raw->main_thread_stacksize);
		get_variable_by_symbol("sceUserMainThreadCpuAffinityMask", ve, &process_param_raw->main_thread_cpu_affinity_mask);
		get_variable_by_symbol("sceUserMainThreadAttribute", ve, &process_param_raw->main_thread_attribute);
		get_variable_by_symbol("sceKernelPreloadModuleInhibit", ve, &process_param_raw->process_preload_disabled);
		ADDRELA(&process_param_raw->main_thread_name);
		ADDRELA(&process_param_raw->main_thread_priority);
		ADDRELA(&process_param_raw->main_thread_stacksize);
		ADDRELA(&process_param_raw->main_thread_cpu_affinity_mask);
		ADDRELA(&process_param_raw->main_thread_attribute);
		ADDRELA(&process_param_raw->process_preload_disabled);
		if (libc_param != NULL) {
			process_param_raw->sce_libc_param = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, size);
			ADDRELA(&process_param_raw->sce_libc_param);
		}

	} else if (params->process_param_size == sizeof(sce_process_param_v5_raw)) {

		sce_process_param_v5_t *process_param;
		sce_process_param_v5_raw *process_param_raw;

		process_param = (sce_process_param_v5_t *)(params->process_param);
		process_param_raw = (sce_process_param_v5_raw *)(ADDR(sceModuleInfo_rodata) + process_param_offset);

		CONVERT32(process_param, size);
		CONVERT32(process_param, magic);
		CONVERT32(process_param, version);
		CONVERT32(process_param, fw_version);
		get_variable_by_symbol("sceUserMainThreadName", ve, &process_param_raw->main_thread_name);
		get_variable_by_symbol("sceUserMainThreadPriority", ve, &process_param_raw->main_thread_priority);
		get_variable_by_symbol("sceUserMainThreadStackSize", ve, &process_param_raw->main_thread_stacksize);
		get_variable_by_symbol("sceUserMainThreadCpuAffinityMask", ve, &process_param_raw->main_thread_cpu_affinity_mask);
		get_variable_by_symbol("sceUserMainThreadAttribute", ve, &process_param_raw->main_thread_attribute);
		get_variable_by_symbol("sceKernelPreloadModuleInhibit", ve, &process_param_raw->process_preload_disabled);
		ADDRELA(&process_param_raw->main_thread_name);
		ADDRELA(&process_param_raw->main_thread_priority);
		ADDRELA(&process_param_raw->main_thread_stacksize);
		ADDRELA(&process_param_raw->main_thread_cpu_affinity_mask);
		ADDRELA(&process_param_raw->main_thread_attribute);
		ADDRELA(&process_param_raw->process_preload_disabled);
		if (libc_param != NULL) {
			process_param_raw->sce_libc_param = VADDR(sceModuleInfo_rodata) + libc_param_offset + offsetof(sce_libc_param_raw, size);
			ADDRELA(&process_param_raw->sce_libc_param);
		}
	} else {
		FAILX("Unknown process_param size (0x%lX)", params->process_param_size);
	}

	module_info_raw = (sce_module_info_raw *)ADDR(sceModuleInfo_rodata);
	CONVERT16(module_info, attributes);
	CONVERT16(module_info, version);
	memcpy(module_info_raw->name, module_info->name, 27);
	module_info_raw->type = module_info->type;
	module_info_raw->export_top = htole32(OFFSET(sceLib_ent));
	module_info_raw->export_end = htole32(OFFSET(sceLib_ent) + sizes->sceLib_ent);
	module_info_raw->import_top = htole32(OFFSET(sceLib_stubs));
	module_info_raw->import_end = htole32(OFFSET(sceLib_stubs) + sizes->sceLib_stubs);
	CONVERT32(module_info, module_nid);
	CONVERT32(module_info, tls_start);
	CONVERT32(module_info, tls_filesz);
	CONVERT32(module_info, tls_memsz);
	if(module_info->module_start != NULL){
		CONVERTOFFSET(module_info, module_start);
	}else{
		module_info_raw->module_start = 0xFFFFFFFF;
		module_info_raw->import_top = 0;
		module_info_raw->import_end = 0;
	}
	if(module_info->module_stop != NULL){
		CONVERTOFFSET(module_info, module_stop);
	}else{
		module_info_raw->module_stop = 0xFFFFFFFF;
	}
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
		CONVERT32(export, num_syms_tls_vars);
		CONVERT32(export, library_nid);
		if (export->library_name != NULL) {
			SETLOCALPTR(export_raw->library_name, sceExport_rodata);
			void *dst = ADDR(sceExport_rodata);
			INCR(sceExport_rodata, ALIGN_4(strlen(export->library_name) + 1));
			strcpy(dst, export->library_name);
		}
		num_syms = export->num_syms_funcs + export->num_syms_vars + export->num_syms_tls_vars;
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
			} else if (export->entry_table[i] == params->process_param) {
				raw_entries[i] = htole32(VADDR(sceModuleInfo_rodata) + process_param_offset);
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
		CONVERT16(import, num_syms_tls_vars);
		CONVERT32(import, reserved1);
		CONVERT32(import, reserved2);
		CONVERT32(import, library_nid);

		if (import->library_name != NULL) {
			SETLOCALPTR(import_raw->library_name, sceImport_rodata);
			void *dst = ADDR(sceImport_rodata);
			INCR(sceImport_rodata, ALIGN_4(strlen(import->library_name) + 1));
			strcpy(dst, import->library_name);
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
			vstub_vaddr = VADDR(sceVStub_rodata) + import->num_syms_vars * 4;
			vstub_addr = ADDR(sceVStub_rodata) + import->num_syms_vars * 4;
			vstubs_size = 0;
			for (i = 0; i < import->num_syms_vars; i++) {
				INTADDR(sceVNID_rodata) = htole32(import->var_nid_table[i]);
				INCR(sceVNID_rodata, 4);
				INTADDR(sceVStub_rodata) = htole32(vstub_vaddr);
				ADDRELA(ADDR(sceVStub_rodata));
				INCR(sceVStub_rodata, 4);

				for (import_vstub = ve->vstubs, j = 0; j < ve->num_vstubs; import_vstub++, j++) {
					if (import_vstub->library_nid == import->library_nid && import_vstub->target_nid == import->var_nid_table[i])
						break;
				}
				if (j == ve->num_vstubs)
					FAILX("Cannot find stub for variable import NID 0x%08X in library %s", import->var_nid_table[i], import->library_name);

				vstub_vaddr += import_vstub->rel_info_size + sizeof(Elf32_Word);
				vstubs_size += import_vstub->rel_info_size + sizeof(Elf32_Word);
				*vstub_addr++ = (import_vstub->rel_info_size + sizeof(Elf32_Word)) << 4; /* Write the header for the relocation info (size of relocation info + 4 bytes for the header itself) */

				if ((vstub_addr[-1] >> 4) > 0xFFFFFF)
					FAILX("Relocation info for variable import NID 0x%08X in library %s is too large (%u bytes)", import_vstub->target_nid, import->library_name, import_vstub->rel_info_size + 4);

				for (j = 0; j < import_vstub->rel_count; j++) {
					vstub_rel = (SCE_Rel *)(import_vstub->rel_info) + j;
					if (vstub_rel->r_short == 1) {
						*vstub_addr++ = htole32(vstub_rel->r_variable_short_entry.r_short |
												(vstub_rel->r_variable_short_entry.r_datseg << 4) | 
												(vstub_rel->r_variable_short_entry.r_code << 8) |
												(vstub_rel->r_variable_short_entry.r_addend << 16));
						*vstub_addr++ = htole32(vstub_rel->r_variable_short_entry.r_offset);
					}
					else {
						*vstub_addr++ = htole32(vstub_rel->r_variable_long_entry.r_short |
												(vstub_rel->r_variable_long_entry.r_datseg << 4) |
												(vstub_rel->r_variable_long_entry.r_code << 8));
						*vstub_addr++ = htole32(vstub_rel->r_variable_long_entry.r_offset);
						*vstub_addr++ = htole32(vstub_rel->r_variable_long_entry.r_addend);
					}
				}
			}

			INCR(sceVStub_rodata, vstubs_size);
		}
		if (import->num_syms_tls_vars) {
			SETLOCALPTR(import_raw->tls_var_nid_table, sceImport_rodata);
			for (i = 0; i < import->num_syms_tls_vars; i++) {
				INTADDR(sceImport_rodata) = htole32(import->var_nid_table[i]);
				INCR(sceImport_rodata, 4);
			}
			SETLOCALPTR(import_raw->tls_var_entry_table, sceImport_rodata);
			for (i = 0; i < import->num_syms_tls_vars; i++) {
				INTADDR(sceImport_rodata) = htole32(vita_elf_host_to_vaddr(ve, import->var_entry_table[i]));
				ADDRELA(ADDR(sceImport_rodata));
				INCR(sceImport_rodata, 4);
			}
		}
	}

	INCR(sceModuleInfo_rodata, sizeof(sce_module_info_raw));
	INCR(sceModuleInfo_rodata, params->process_param_size);
	if (libc_param != NULL)
		INCR(sceModuleInfo_rodata, sizeof(sce_libc_param_raw));

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
	start_segoffset = (start_segoffset + 0xF) & ~0xF; // align to 16 bytes, same with `sce_elf_module_info_encode`
	total_size += (start_segoffset - ve->segments[segndx].memsz); // add the padding size

	start_vaddr = segment_base + start_segoffset;
	start_foffset = phdr.p_offset + start_segoffset;
	cur_pos = 0;

	if (!elf_utils_shift_contents(dest, start_foffset, total_size))
		FAILX("Unable to relocate ELF sections");

	/* Extend in our copy of phdrs so that vita_elf_vaddr_to_segndx can match it */
	ve->segments[segndx].memsz += total_size;

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

/* We have to check all relocs. If any of the point to a space in ELF that is not contained in any segment,
 * we should discard this reloc. This should be done before we extend the code segment with modinfo, because otherwise
 * the invalid addresses may become valid */
int sce_elf_discard_invalid_relocs(const vita_elf_t *ve, vita_elf_rela_table_t *rtable) {
	vita_elf_rela_table_t *curtable;
	vita_elf_rela_t *vrela;
	int i, datseg;
	for (curtable = rtable; curtable; curtable = curtable->next) {
		for (i = 0, vrela = curtable->relas; i < curtable->num_relas; i++, vrela++) {
			if (vrela->type == R_ARM_NONE || (vrela->symbol && vrela->symbol->shndx == 0)) {
				vrela->type = R_ARM_NONE;
				continue;
			}
			/* We skip relocations that are not real relocations 
			 * In all current tested output, we have that the unrelocated value is correct. 
			 * However, there is nothing that says this has to be the case. SCE RELS 
			 * does not support ABS value relocations anymore, so there's not much 
			 * we can do. */
			// TODO: Consider a better solution for this.
			if (vrela->symbol && (vrela->symbol->shndx == SHN_ABS || vrela->symbol->shndx == SHN_COMMON)) {
				vrela->type = R_ARM_NONE;
				continue;
			}
			datseg = vita_elf_vaddr_to_segndx(ve, vrela->offset);
			/* We can get -1 here for some debugging-related relocations.
			 * These are done against debug sections that aren't mapped to any segment.
			 * Just ignore these */
			if (datseg == -1)
				vrela->type = R_ARM_NONE;
		}
	}
	return 1;
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

	// sce_rel_func = sce_rel_short;
	sce_rel_func = sce_rel_long;

encode_relas:
	curpos = encoded_relas;

	for (curtable = rtable; curtable; curtable = curtable->next) {
		for (i = 0, vrela = curtable->relas; i < curtable->num_relas; i++, vrela++) {
			if (vrela->type == R_ARM_NONE)
				continue;
			datseg = vita_elf_vaddr_to_segndx(ve, vrela->offset);
			datoff = vita_elf_vaddr_to_segoffset(ve, vrela->offset, datseg);
			if (vrela->symbol) {
				symvaddr = vrela->symbol->value + vrela->addend;
			} else {
				symvaddr = vrela->addend;
			}
			symseg = vita_elf_vaddr_to_segndx(ve, vrela->symbol ? vrela->symbol->value : vrela->addend);
			if (symseg == -1)
				continue;
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

static int update_symtab_entries(Elf_Scn *scn, int cur_ndx)
{
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	int total_bytes;
	int data_beginsym, symndx;
	vita_elf_symbol_t *cursym;

	gelf_getshdr(scn, &shdr);

	data = NULL; total_bytes = 0;
	while (total_bytes < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {

		data_beginsym = data->d_off / shdr.sh_entsize;
		for (symndx = 0; symndx < data->d_size / shdr.sh_entsize; symndx++) {
			if (gelf_getsym(data, symndx, &sym) != &sym)
				FAILE("gelf_getsym() failed");

			/* Symbol is for the deleted section, so invalidate it */
			if (sym.st_shndx == cur_ndx)
				sym.st_shndx = 0;
			else if (sym.st_shndx > cur_ndx)
				sym.st_shndx--;

			ELF_ASSERT(gelf_update_sym(data, symndx, &sym));
		}

		total_bytes += data->d_size;
	}

	return 1;
failure:
	return 0;
}

int sce_elf_rewrite_stubs(Elf *dest, vita_elf_t *ve)
{
	Elf_Scn *scn, *symtab_scn;
	GElf_Shdr shdr;
	Elf_Data *data;
	size_t shstrndx;
	void *shstrtab;
	uint32_t *stubdata;
	int j, i;
	int *cur_ndx;
	char *sh_name, *stub_name;

	ELF_ASSERT(elf_getshdrstrndx(dest, &shstrndx) == 0);
	ELF_ASSERT(scn = elf_getscn(dest, shstrndx));
	ELF_ASSERT(symtab_scn = elf_getscn(dest, ve->symtab_ndx));
	ELF_ASSERT(data = elf_getdata(scn, NULL));
	shstrtab = data->d_buf;

	for(j=0;j<ve->fstubs_va.count;j++) {
		cur_ndx = VARRAY_ELEMENT(&ve->fstubs_va,j);
		ELF_ASSERT(scn = elf_getscn(dest, *cur_ndx));
		ELF_ASSERT(gelf_getshdr(scn, &shdr));
		
		sh_name = shstrtab + shdr.sh_name;
		if (strstr(sh_name, ".vitalink.fstubs.") != sh_name)
			errx(EXIT_FAILURE, "Your ELF file contains a malformed .vitalink.fstubs section. Please make sure all your stub libraries are up-to-date.");
		stub_name = strrchr(sh_name, '.');
		snprintf(sh_name, strlen(sh_name) + 1, ".text.fstubs%s", stub_name);
		
		data = NULL;
		while ((data = elf_getdata(scn, data)) != NULL) {
			for (stubdata = (uint32_t *)data->d_buf;
					(void *)stubdata < data->d_buf + data->d_size - 11; stubdata += 4) {
				stubdata[0] = htole32(sce_elf_stub_func[0]);
				stubdata[1] = htole32(sce_elf_stub_func[1]);
				stubdata[2] = htole32(sce_elf_stub_func[2]);
				stubdata[3] = 0;
			}
		}
	}


	/* If the section index is zero, it means that it's nonexistent */
	if (ve->vstubs_va.count == 0) {
		return 1;
	}
	
	for (j = ve->vstubs_va.count - 1; j >= 0; j--) {
		cur_ndx = VARRAY_ELEMENT(&ve->vstubs_va,j);
		ELF_ASSERT(scn = elf_getscn(dest, *cur_ndx));
		ELF_ASSERT(gelf_getshdr(scn, &shdr));

		/* Remove vstub section */
		elfx_remscn(dest, scn);

		/* Fixup shstrndx */
		if (shstrndx > *cur_ndx)
			elfx_update_shstrndx(dest, --shstrndx);

		if (ve->symtab_ndx > *cur_ndx)
			ve->symtab_ndx--;
		
		/* Fixup section index references */
		scn = NULL;
		while ((scn = elf_nextscn(dest, scn)) != NULL)
		{
			ELF_ASSERT(gelf_getshdr(scn, &shdr));

			if (shdr.sh_link > *cur_ndx)
				shdr.sh_link--;
			if (shdr.sh_info > *cur_ndx)
				shdr.sh_info--;

			gelf_update_shdr(scn, &shdr);
		}

		/* Fixup symbol table section indices */
		update_symtab_entries(symtab_scn, *cur_ndx);

		for (i = j + 1; i < ve->vstubs_va.count; i++) {
			cur_ndx = VARRAY_ELEMENT(&ve->vstubs_va, i);
			(*cur_ndx)--;
		}

		varray_remove(&ve->vstubs_va, j);
	}

	return 1;
failure:
	return 0;
}

int sce_elf_set_headers(FILE *destfile, const vita_elf_t *ve)
{
	Elf32_Ehdr ehdr;

	ehdr.e_type = htole16(ET_SCE_RELEXEC);

	SYS_ASSERT(fseek(destfile, offsetof(Elf32_Ehdr, e_type), SEEK_SET));
	SYS_ASSERT(fwrite(&ehdr.e_type, sizeof(ehdr.e_type), 1, destfile));

	return 1;
failure:
	return 0;
}
