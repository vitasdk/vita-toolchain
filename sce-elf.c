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
