#ifndef VITA_IMPORT_H
#define VITA_IMPORT_H

#include <stdint.h>

typedef struct {
	char *name;
	uint32_t NID;
} vita_imports_stub_t;

typedef struct {
	char *name;
	uint32_t NID;
	vita_imports_stub_t **functions;
	vita_imports_stub_t **variables;
	int n_functions;
	int n_variables;
} vita_imports_lib_t;

typedef struct {
	char *name;
	uint32_t NID;
	vita_imports_lib_t **libs;
	int n_libs;
} vita_imports_module_t;

typedef struct {
	vita_imports_module_t **modules;
	int n_modules;
} vita_imports_t;


vita_imports_t *vita_imports_new(int n_modules);
void vita_imports_free(vita_imports_t *imp);

vita_imports_module_t *vita_imports_module_new(const char *name, uint32_t NID, int n_libs);
void vita_imports_module_free(vita_imports_module_t *mod);

vita_imports_lib_t *vita_imports_lib_new(const char *name, uint32_t NID, int n_functions, int n_variables);
void vita_imports_lib_free(vita_imports_lib_t *lib);

vita_imports_stub_t *vita_imports_stub_new(const char *name, uint32_t NID);
void vita_imports_stub_free(vita_imports_stub_t *stub);

#endif
