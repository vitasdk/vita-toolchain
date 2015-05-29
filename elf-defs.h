#ifndef ELF_DEFS_H
#define ELF_DEFS_H

#ifndef R_ARM_THM_CALL
# define R_ARM_THM_CALL 10
#endif

/* Decode functions to turn an enum into a string */

const char *elf_decode_e_type(int e_type);	/* ELF file type */
const char *elf_decode_sh_type(int sh_type);	/* Section header type */
const char *elf_decode_p_type(int p_type);	/* Program segment type */
const char *elf_decode_st_bind(int st_bind);	/* Symbol binding */
const char *elf_decode_st_type(int st_type);	/* Symbol type */
const char *elf_decode_r_type(int r_type);	/* Relocation type */

#endif
