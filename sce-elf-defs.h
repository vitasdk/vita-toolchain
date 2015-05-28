/* This file gets included multiple times to generate the host-visible and target-visible versions of each struct */

#if defined(SCE_ELF_DEFS_HOST)
# define SCE_TYPE(type) type
# define SCE_PTR(type) type
#elif defined(SCE_ELF_DEFS_TARGET)
# define SCE_TYPE(type) type ## _raw
# define SCE_PTR(type) uint32_t
#else
# error "Do not include sce-elf-defs.h directly!  Include elf-defs.h!"
#endif

#include <stdint.h>

typedef struct {
	uint16_t attributes;
	uint16_t version;	/* Set to 0x0101 */
	char name[27];		/* Name of the library */
	uint8_t type;		/* 0x0 for executable, 0x6 for PRX */
	SCE_PTR(void *) gp_value;
	uint32_t export_top;	/* Offset to start of export table */
	uint32_t export_end;	/* Offset to end of export table */
	uint32_t import_top;	/* Offset to start of import table */
	uint32_t import_end;	/* Offset to end of import table */
	uint32_t library_nid;	/* NID of this library */
	uint32_t field_38;
	uint32_t field_3C;
	uint32_t field_40;
	uint32_t module_start;	/* Offset to function to run when library is started, 0 to disable */
	uint32_t module_stop;	/* Offset to function to run when library is exiting, 0 to disable */
	uint32_t exidx_top;	/* Offset to start of ARM EXIDX (optional) */
	uint32_t exidx_end;	/* Offset to end of ARM EXIDX (optional) */
	uint32_t extab_top;	/* Offset to start of ARM EXTAB (optional) */
	uint32_t extab_end;	/* Offset to end of ARM EXTAB (optional */
} SCE_TYPE(sce_module_info);

typedef struct {
	uint16_t size;			/* Size of this struct, set to 0x20 */
	uint16_t version;		/* 0x1 for normal export, 0x0 for main module export */
	uint16_t flags;			/* 0x1 for normal export, 0x8000 for main module export */
	uint16_t num_syms_funcs;	/* Number of function exports */
	uint32_t num_syms_vars;		/* Number of variable exports */
	uint32_t num_syms_unk;
	uint32_t module_nid;		/* NID of this module */
	SCE_PTR(char *) module_name;	/* Pointer to name of this module */
	uint32_t *nid_table;		/* Pointer to array of 32-bit NIDs to export */
	SCE_PTR(void **) entry_table;	/* Pointer to array of data pointers for each NID */
} SCE_TYPE(sce_module_exports);

typedef struct {
	uint16_t size;				/* Size of this struct, set to 0x34 */
	uint16_t version;			/* Set to 0x1 */
	uint16_t flags;				/* Set to 0x0 */
	uint16_t num_syms_funcs;		/* Number of function imports */
	uint16_t num_syms_vars;			/* Number of variable imports */
	uint16_t num_syms_unk;
	uint32_t reserved1;
	uint32_t module_nid;			/* NID of module to import */
	SCE_PTR(char *) module_name;		/* Pointer to name of imported module, for debugging */
	uint32_t reserved2;
	SCE_PTR(uint32_t *) func_nid_table;	/* Pointer to array of function NIDs to import */
	SCE_PTR(void **) func_entry_table;	/* Pointer to array of stub functions to fill */
	SCE_PTR(uint32_t *) var_nid_table;	/* Pointer to array of variable NIDs to import */
	SCE_PTR(void **) var_entry_table;	/* Pointer to array of data pointers to write to */
	SCE_PTR(uint32_t *) unk_nid_table;
	SCE_PTR(void **) unk_entry_table;
} SCE_TYPE(sce_module_imports);

#undef SCE_TYPE
#undef SCE_PTR
