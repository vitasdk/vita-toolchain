#ifndef ELF_DEFS_H
#define ELF_DEFS_H

# define R_ARM_NONE            0
# define R_ARM_ABS32           2
# define R_ARM_REL32           3
# define R_ARM_THM_CALL        10
# define R_ARM_CALL            28
# define R_ARM_JUMP24          29
# define R_ARM_THM_JUMP24      30
# define R_ARM_TARGET1         38
# define R_ARM_SBREL31         39
# define R_ARM_V4BX            40
# define R_ARM_TARGET2         41
# define R_ARM_PREL31          42
# define R_ARM_MOVW_ABS_NC     43
# define R_ARM_MOVT_ABS        44
# define R_ARM_MOVW_PREL_NC    45
# define R_ARM_MOVT_PREL       46
# define R_ARM_THM_MOVW_ABS_NC 47
# define R_ARM_THM_MOVT_ABS    48
# define R_ARM_THM_PC11        102

#define STB_NUM     3
#ifndef SHT_ARM_EXIDX
#define SHT_ARM_EXIDX 0x70000001
#endif
/* Decode functions to turn an enum into a string */

const char *elf_decode_e_type(int e_type);	/* ELF file type */
const char *elf_decode_sh_type(int sh_type);	/* Section header type */
const char *elf_decode_p_type(int p_type);	/* Program segment type */
const char *elf_decode_st_bind(int st_bind);	/* Symbol binding */
const char *elf_decode_st_type(int st_type);	/* Symbol type */
const char *elf_decode_r_type(int r_type);	/* Relocation type */

#endif
