#include "vita-import.h"
#include <stdlib.h>
#include <string.h>

vita_imports_t *vita_imports_new(int n_modules)
{
	vita_imports_t *imp = malloc(sizeof(*imp));
	if (imp == NULL)
		return NULL;

	imp->postfix = calloc(64, sizeof(char));

	imp->firmware = NULL;

	imp->n_modules = n_modules;

	imp->modules = calloc(n_modules, sizeof(*imp->modules));

	return imp;
}

void vita_imports_free(vita_imports_t *imp)
{
	if (imp) {
		int i;
		for (i = 0; i < imp->n_modules; i++) {
			vita_imports_module_free(imp->modules[i]);
		}

		if (imp->firmware)
			free(imp->firmware);

		free(imp->postfix);
		free(imp);
	}
}

vita_imports_module_t *vita_imports_module_new(const char *name, uint32_t NID, int n_modules)
{
	vita_imports_module_t *mod = malloc(sizeof(*mod));
	if (mod == NULL)
		return NULL;

	mod->name = strdup(name);
	mod->NID = NID;
	mod->n_libs = n_modules;

	mod->libs = calloc(n_modules, sizeof(*mod->libs));

	return mod;
}


vita_imports_lib_t *vita_imports_lib_new(const char *name, bool kernel, uint32_t NID, int n_functions, int n_variables)
{
	vita_imports_lib_t *lib = malloc(sizeof(*lib));
	if (lib == NULL)
		return NULL;

	lib->name = strdup(name);
	lib->NID = NID;
	lib->is_kernel = kernel;
	lib->n_functions = n_functions;
	lib->n_variables = n_variables;

	lib->functions = calloc(n_functions, sizeof(*lib->functions));

	lib->variables = calloc(n_variables, sizeof(*lib->variables));

	return lib;
}

void vita_imports_lib_free(vita_imports_lib_t *lib)
{
	if (lib) {
		int i;
		for (i = 0; i < lib->n_variables; i++) {
			vita_imports_stub_free(lib->variables[i]);
		}
		for (i = 0; i < lib->n_functions; i++) {
			vita_imports_stub_free(lib->functions[i]);
		}
		free(lib->name);
		free(lib);
	}
}


void vita_imports_module_free(vita_imports_module_t *mod)
{
	if (mod) {
		int i;
		for (i = 0; i < mod->n_libs; i++) {
			vita_imports_lib_free(mod->libs[i]);
		}
		free(mod->name);
		free(mod);
	}
}

vita_imports_stub_t *vita_imports_stub_new(const char *name, uint32_t NID)
{
	vita_imports_stub_t *stub = malloc(sizeof(*stub));
	if (stub == NULL)
		return NULL;

	stub->name = strdup(name);
	stub->NID = NID;

	return stub;
}

void vita_imports_stub_free(vita_imports_stub_t *stub)
{
	if (stub) {
		free(stub->name);
		free(stub);
	}
}

/* For now these functions are just dumb full-table searches.  We can implement qsort/bsearch/whatever later if necessary. */

static vita_imports_common_fields *generic_find(vita_imports_common_fields **entries, int n_entries, uint32_t NID) {
	int i;
	vita_imports_common_fields *entry;

	for (i = 0; i < n_entries; i++) {
		entry = entries[i];
		if (entry == NULL)
			continue;

		if (entry->NID == NID)
			return entry;
	}

	return NULL;
}

vita_imports_module_t *vita_imports_find_module(vita_imports_t *imp, uint32_t NID) {
	return (vita_imports_module_t *)generic_find((vita_imports_common_fields **)imp->modules, imp->n_modules, NID);
}
vita_imports_lib_t *vita_imports_find_lib(vita_imports_module_t *mod, uint32_t NID) {
	return (vita_imports_lib_t *)generic_find((vita_imports_common_fields **)mod->libs, mod->n_libs, NID);
}
vita_imports_stub_t *vita_imports_find_function(vita_imports_lib_t *lib, uint32_t NID) {
	return (vita_imports_stub_t *)generic_find((vita_imports_common_fields **)lib->functions, lib->n_functions, NID);
}
vita_imports_stub_t *vita_imports_find_variable(vita_imports_lib_t *lib, uint32_t NID) {
	return (vita_imports_stub_t *)generic_find((vita_imports_common_fields **)lib->variables, lib->n_variables, NID);
}
