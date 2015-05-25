#include "vita-import.h"
#include <stdlib.h>
#include <string.h>

vita_imports_t *vita_imports_new(int n_libs)
{
	vita_imports_t *imp = malloc(sizeof(*imp));
	if (imp == NULL)
		return NULL;

	imp->n_libs = n_libs;

	imp->libs = malloc(n_libs * sizeof(*imp->libs));
	memset(imp->libs, 0, n_libs * sizeof(*imp->libs));

	return imp;
}

void vita_imports_free(vita_imports_t *imp)
{
	if (imp) {
		int i;
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_free(imp->libs[i]);
		}
		free(imp);
	}
}

vita_imports_lib_t *vita_imports_lib_new(const char *name, uint32_t NID, int n_modules)
{
	vita_imports_lib_t *lib = malloc(sizeof(*lib));
	if (lib == NULL)
		return NULL;

	lib->name = strdup(name);
	lib->NID = NID;
	lib->n_modules = n_modules;

	lib->modules = malloc(n_modules * sizeof(*lib->modules));
	memset(lib->modules, 0, n_modules * sizeof(*lib->modules));

	return lib;
}


vita_imports_module_t *vita_imports_module_new(const char *name, uint32_t NID, int n_functions, int n_variables)
{
	vita_imports_module_t *mod = malloc(sizeof(*mod));
	if (mod == NULL)
		return NULL;

	mod->name = strdup(name);
	mod->NID = NID;
	mod->n_functions = n_functions;
	mod->n_variables = n_variables;

	mod->functions = malloc(n_functions * sizeof(*mod->functions));
	memset(mod->functions, 0, n_functions * sizeof(*mod->functions));

	mod->variables = malloc(n_variables * sizeof(*mod->variables));
	memset(mod->variables, 0, n_variables * sizeof(*mod->variables));

	return mod;
}

void vita_imports_module_free(vita_imports_module_t *mod)
{
	if (mod) {
		int i;
		for (i = 0; i < mod->n_variables; i++) {
			vita_imports_stub_free(mod->variables[i]);
		}
		for (i = 0; i < mod->n_functions; i++) {
			vita_imports_stub_free(mod->functions[i]);
		}
		free(mod->name);
		free(mod);
	}
}


void vita_imports_lib_free(vita_imports_lib_t *lib)
{
	if (lib) {
		int i;
		for (i = 0; i < lib->n_modules; i++) {
			vita_imports_module_free(lib->modules[i]);
		}
		free(lib->name);
		free(lib);
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
