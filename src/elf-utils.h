#ifndef ELF_UTILS_H
#define ELF_UTILS_H

#include <stdio.h>
#include <libelf.h>

#define ASSERT(cond,fmt...) if(!(cond -0)){fprintf(stderr,"Failure:"#cond "\n" fmt);goto failure;}
#define ELF_ASSERT(cond)    ASSERT((cond)!=0,"%s",elf_errmsg(-1))

int elf_utils_copy(Elf *dest, Elf *source);

Elf *elf_utils_copy_to_file(const char *filename, Elf *source, FILE **file);

int elf_utils_duplicate_scn_contents(Elf *e, int scndx);
int elf_utils_duplicate_shstrtab(Elf *e);
void elf_utils_free_scn_contents(Elf *e, int scndx);

int elf_utils_shift_contents(Elf *e, int start_offset, int shift_amount);

Elf_Scn *elf_utils_new_scn_with_name(Elf *e, const char *scn_name);

Elf_Scn *elf_utils_new_scn_with_data(Elf *e, const char *scn_name, void *buf, int len);

#endif
