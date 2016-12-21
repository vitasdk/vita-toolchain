#ifndef SCE_ELF_H
#define SCE_ELF_H

#include "velf.h"

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

#define NID_MODULE_STOP		0x79F8E492
#define NID_MODULE_EXIT		0x913482A9
#define NID_MODULE_START	0x935CD196
#define NID_MODULE_INFO		0x6C2224BA
#define NID_PROCESS_PARAM	0x70FBA1E7

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

/* This struct must only contain Elf32_Words, because we use it as an array in sce-elf.c */
typedef struct {
	Elf32_Word sceModuleInfo_rodata;	/* The sce_module_info structure */
	Elf32_Word sceLib_ent;			/* All sce_module_exports structures */
	Elf32_Word sceExport_rodata;		/* The tables referenced by sce_module_exports */
	Elf32_Word sceLib_stubs;		/* All sce_module_imports structures */
	Elf32_Word sceImport_rodata;		/* Misc data referenced by sce_module_imports */
	Elf32_Word sceFNID_rodata;		/* The imported function NID arrays */
	Elf32_Word sceFStub_rodata;		/* The imported function pointer arrays */
	Elf32_Word sceVNID_rodata;		/* The imported function NID arrays */
	Elf32_Word sceVStub_rodata;		/* The imported function NID arrays */
} sce_section_sizes_t;

sce_module_info_t *sce_elf_module_info_create(vita_elf_t *ve, vita_export_t *exports);

int sce_elf_module_info_get_size(sce_module_info_t *module_info, sce_section_sizes_t *sizes);

void sce_elf_module_info_free(sce_module_info_t *module_info);

void *sce_elf_module_info_encode(
		const sce_module_info_t *module_info, const vita_elf_t *ve, const sce_section_sizes_t *sizes,
		vita_elf_rela_table_t *rtable);

int sce_elf_write_module_info(
		Elf *dest, const vita_elf_t *ve, const sce_section_sizes_t *sizes, void *module_info);

int sce_elf_discard_invalid_relocs(const vita_elf_t *ve, vita_elf_rela_table_t *rtable);

int sce_elf_write_rela_sections(
		Elf *dest, const vita_elf_t *ve, const vita_elf_rela_table_t *rtable);

int sce_elf_rewrite_stubs(Elf *dest, const vita_elf_t *ve);

int sce_elf_set_headers(FILE *outfile, const vita_elf_t *ve);

#endif
