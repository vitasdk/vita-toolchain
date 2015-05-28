#ifndef SCE_ELF_H
#define SCE_ELF_H

#include <elf.h>
#include <stdint.h>

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

/* SCE-specific definitions for e_type: */
#define ET_SCE_EXEC		0xFE00		/* SCE Executable file */
#define ET_SCE_RELEXEC		0xFE04		/* SCE Relocatable file */
#define ET_SCE_STUBLIB		0xFE0C		/* SCE SDK Stubs */
#define ET_SCE_DYNAMIC		0xFE18		/* Unused */
#define ET_SCE_PSPRELEXEC	0xFFA0		/* Unused (PSP ELF only) */
#define ET_SCE_PPURELEXEC	0xFFA4		/* Unused (SPU ELF only) */
#define ET_SCE_UNK		0xFFA5		/* Unknown */

/* SCE-specific definitions for sh_type: */
#define SHT_SCE_RELA		0x60000000	/* SCE Relocations */
#define SHT_SCENID		0x61000001	/* Unused (PSP ELF only) */
#define SHT_SCE_PSPRELA		0x700000A0	/* Unused (PSP ELF only) */
#define SHT_SCE_ARMRELA		0x700000A4	/* Unused (PSP ELF only) */

/* SCE-specific definitions for p_type: */
#define PT_SCE_RELA		0x60000000	/* SCE Relocations */
#define PT_SCE_COMMENT		0x6FFFFF00	/* Unused */
#define PT_SCE_VERSION		0x6FFFFF01	/* Unused */
#define PT_SCE_UNK		0x70000001	/* Unknown */
#define PT_SCE_PSPRELA		0x700000A0	/* Unused (PSP ELF only) */
#define PT_SCE_PPURELA		0x700000A4	/* Unused (SPU ELF only) */

typedef union {
	Elf32_Word r_short : 4;
	struct {
		Elf32_Word r_short     : 4;
		Elf32_Word r_symseg    : 4;
		Elf32_Word r_code      : 8;
		Elf32_Word r_datseg    : 4;
		Elf32_Word r_offset_lo : 12;
		Elf32_Word r_offset_hi : 20;
		Elf32_Word r_addend    : 12;
	} r_short_entry;
	struct {
		Elf32_Word r_short     : 4;
		Elf32_Word r_symseg    : 4;
		Elf32_Word r_code      : 8;
		Elf32_Word r_datseg    : 4;
		Elf32_Word r_code2     : 8;
		Elf32_Word r_dist2     : 4;
		Elf32_Word r_addend;
		Elf32_Word r_offset;
	} r_long_entry;
	struct {
		Elf32_Word r_word1;
		Elf32_Word r_word2;
		Elf32_Word r_word3;
	} r_raw_entry;
} SCE_Rel;

#define SCE_ELF_DEFS_HOST
#include "sce-elf-defs.h"
#undef SCE_ELF_DEFS_HOST

#define SCE_ELF_DEFS_TARGET
#include "sce-elf-defs.h"
#undef SCE_ELF_DEFS_TARGET

#endif
