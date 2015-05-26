#ifndef VITA_ELF_H
#define VITA_ELF_H

#include <libelf.h>

typedef struct vita_elf {
	int fd;
	int mode;
	Elf *elf;

	int fstubs_ndx;
	int vstubs_ndx;
} vita_elf_t;

vita_elf_t *vita_elf_load(const char *filename);
void vita_elf_free(vita_elf_t *ve);

#endif
