#ifndef ELF_UTILS_H
#define ELF_UTILS_H

#include <libelf.h>

int elf_utils_copy(Elf *dest, Elf *source);

Elf *elf_utils_copy_to_file(const char *filename, Elf *source, int *fd);

int elf_utils_duplicate_scn_contents(Elf *e, int scndx);
int elf_utils_duplicate_shstrtab(Elf *e);
void elf_utils_free_scn_contents(Elf *e, int scndx);

int elf_utils_shift_contents(Elf *e, int start_offset, int shift_amount);

Elf_Scn *elf_utils_new_scn_with_name(Elf *e, const char *scn_name);

Elf_Scn *elf_utils_new_scn_with_data(Elf *e, const char *scn_name, void *buf, int len);

#endif
