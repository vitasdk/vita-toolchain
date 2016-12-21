#ifndef VITA_ELF_H
#define VITA_ELF_H

#include <stdio.h>
#include <libelf.h>
#include <stdint.h>
#include <stdbool.h>

#include "import.h"
#include "varray.h"

/* Convenience representation of a symtab entry */
typedef struct vita_elf_symbol_t {
	const char *name;
	Elf32_Addr value;
	uint8_t type;
	uint8_t binding;
	int shndx;
} vita_elf_symbol_t;

typedef struct vita_elf_rela_t {
	uint8_t type;
	vita_elf_symbol_t *symbol;
	Elf32_Addr offset;
	Elf32_Sword addend;
} vita_elf_rela_t;

typedef struct vita_elf_rela_table_t {
	vita_elf_rela_t *relas;
	int num_relas;

	int target_ndx;

	struct vita_elf_rela_table_t *next;
} vita_elf_rela_table_t;

typedef struct vita_elf_stub_t {
	Elf32_Addr addr;
	uint32_t module_nid;
	uint32_t target_nid;

	vita_elf_symbol_t *symbol;

	vita_imports_module_t *module;
	vita_imports_stub_t *target;
} vita_elf_stub_t;

typedef struct vita_elf_segment_info_t {
	Elf32_Word type;	/* Segment type */
	Elf32_Addr vaddr;	/* Top of segment space on TARGET */
	Elf32_Word memsz;	/* Size of segment space */

	/* vaddr_top/vaddr_bottom point to a reserved, unallocated memory space that
	 * represents the segment space in the HOST.  This space can be used as
	 * pointer targets for translated data structures. */
	const void *vaddr_top;
	const void *vaddr_bottom;
} vita_elf_segment_info_t;

typedef struct vita_elf_t {
	FILE *file;
	int mode;
	Elf *elf;

	varray fstubs_va;
	varray vstubs_va;

	int symtab_ndx;
	vita_elf_symbol_t *symtab;
	int num_symbols;

	vita_elf_rela_table_t *rela_tables;

	vita_elf_stub_t *fstubs;
	vita_elf_stub_t *vstubs;
	int num_fstubs;
	int num_vstubs;

	vita_elf_segment_info_t *segments;
	int num_segments;
} vita_elf_t;

vita_elf_t *vita_elf_load(const char *filename, bool check_stub_count);
void vita_elf_free(vita_elf_t *ve);

int vita_elf_lookup_imports(vita_elf_t *ve);

const void *vita_elf_vaddr_to_host(const vita_elf_t *ve, Elf32_Addr vaddr);
const void *vita_elf_segoffset_to_host(const vita_elf_t *ve, int segndx, uint32_t offset);

Elf32_Addr vita_elf_host_to_vaddr(const vita_elf_t *ve, const void *host_addr);
int vita_elf_host_to_segndx(const vita_elf_t *ve, const void *host_addr);
int32_t vita_elf_host_to_segoffset(const vita_elf_t *ve, const void *host_addr, int segndx);

int vita_elf_vaddr_to_segndx(const vita_elf_t *ve, Elf32_Addr vaddr);
uint32_t vita_elf_vaddr_to_segoffset(const vita_elf_t *ve, Elf32_Addr vaddr, int segndx);

#endif
