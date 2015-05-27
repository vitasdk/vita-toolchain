#ifndef VITA_ELF_H
#define VITA_ELF_H

#include <libelf.h>
#include <stdint.h>

#include "vita-import.h"

typedef struct vita_elf_stub_t {
	Elf32_Addr addr;
	uint32_t library_nid;
	uint32_t module_nid;
	uint32_t target_nid;

	const char *sym_name;

	vita_imports_lib_t *library;
	vita_imports_module_t *module;
	vita_imports_stub_t *target;
} vita_elf_stub_t;

typedef struct vita_elf {
	int fd;
	int mode;
	Elf *elf;

	int fstubs_ndx;
	int vstubs_ndx;

	vita_elf_stub_t *fstubs;
	vita_elf_stub_t *vstubs;
	int num_fstubs;
	int num_vstubs;
} vita_elf_t;

vita_elf_t *vita_elf_load(const char *filename);
void vita_elf_free(vita_elf_t *ve);

int vita_elf_lookup_imports(vita_elf_t *ve, vita_imports_t *imports);

#endif
