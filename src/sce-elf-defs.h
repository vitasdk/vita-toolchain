/* This file gets included multiple times to generate the host-visible and target-visible versions of each struct */

#if defined(SCE_ELF_DEFS_HOST)
# define SCE_TYPE(type) type ## _t
# define SCE_PTR(type) type
#elif defined(SCE_ELF_DEFS_TARGET)
# define SCE_TYPE(type) type ## _raw
# define SCE_PTR(type) uint32_t
#else
# error "Do not include sce-elf-defs.h directly!  Include sce-elf.h!"
#endif

#include <stdint.h>

struct SCE_TYPE(sce_module_exports);
struct SCE_TYPE(sce_module_imports);

typedef struct SCE_TYPE(sce_module_info) {
	uint16_t attributes;
	uint16_t version;			/* Set to 0x0101 */
	char name[27];				/* Name of the library */
	uint8_t type;				/* 0x0 for executable, 0x6 for PRX */
	SCE_PTR(const void *) gp_value;
	SCE_PTR(struct sce_module_exports_t *)
		export_top;			/* Offset to start of export table */
	SCE_PTR(struct sce_module_exports_t *)
		export_end;			/* Offset to end of export table */
	SCE_PTR(struct sce_module_imports_t *)
		import_top;			/* Offset to start of import table */
	SCE_PTR(struct sce_module_imports_t *)
		import_end;			/* Offset to end of import table */
	uint32_t module_nid;			/* NID of this module */
	uint32_t tls_start;
	uint32_t tls_filesz;
	uint32_t tls_memsz;
	SCE_PTR(const void *) module_start;	/* Offset to function to run when library is started, 0 to disable */
	SCE_PTR(const void *) module_stop;	/* Offset to function to run when library is exiting, 0 to disable */
	SCE_PTR(const void *) exidx_top;	/* Offset to start of ARM EXIDX (optional) */
	SCE_PTR(const void *) exidx_end;	/* Offset to end of ARM EXIDX (optional) */
	SCE_PTR(const void *) extab_top;	/* Offset to start of ARM EXTAB (optional) */
	SCE_PTR(const void *) extab_end;	/* Offset to end of ARM EXTAB (optional */

	// i decided to include process param into module_info (xyz)
	uint32_t process_param_size;                             /* 0x34 */
	uint32_t process_param_magic;                            /* PSP2 */
	uint32_t process_param_ver;                              /* ? 6 ? */
	uint32_t process_param_fw_ver;                           /* sdk vsersion */
	SCE_PTR(const char *) process_param_main_thread_name;    /* thread name pointer*/
	SCE_PTR(int32_t *) process_param_main_thread_priority;   /* thread initPriority */
	SCE_PTR(uint32_t *) process_param_main_thread_stacksize; /* thread stacksize*/
	uint32_t process_param_main_thread_attribute;            /* unknown */
	SCE_PTR(const char *) process_param_process_name;        /* process name pointer */
	/*
	 * 0x01000000 - SceLibPvf
	 * 0x00800000 - SceLibft2
	 * 0x00400000 - SceAppUtil
	 * 0x00200000 - unknown
	 * 0x00100000 - SceCommonDialog
	 * 0x00080000 - SceShellSvc
	 * 0x00040000 - unknown
	 * 0x00020000 - SceLibDbg and SceLibPvf
	 */
	SCE_PTR(uint32_t *) process_param_process_preload_disabled; /* module load inhibit */
	uint32_t process_param_main_thread_cpu_affinity_mask;       /* unknown */
	SCE_PTR(const void *) process_param_sce_libc_param;         /* SceLibc param pointer */
	uint32_t process_param_unk;                                 /* unknown */
} SCE_TYPE(sce_module_info);

typedef struct SCE_TYPE(sce_module_exports) {
	uint16_t size;				/* Size of this struct, set to 0x20 */
	uint16_t version;			/* 0x1 for normal export, 0x0 for main module export */
	uint16_t flags;				/* 0x1 for normal export, 0x8000 for main module export */
	uint16_t num_syms_funcs;		/* Number of function exports */
	uint32_t num_syms_vars;			/* Number of variable exports */
	uint32_t num_syms_tls_vars;     /* Number of TLS variable exports */
	uint32_t library_nid;			/* NID of this library */
	SCE_PTR(const char *) library_name;	/* Pointer to name of this library */
	SCE_PTR(uint32_t *) nid_table;		/* Pointer to array of 32-bit NIDs to export */
	SCE_PTR(const void **) entry_table;	/* Pointer to array of data pointers for each NID */
} SCE_TYPE(sce_module_exports);

typedef struct SCE_TYPE(sce_module_imports) {
	uint16_t size;				/* Size of this struct, set to 0x34 */
	uint16_t version;			/* Set to 0x1 */
	uint16_t flags;				/* Set to 0x0 */
	uint16_t num_syms_funcs;		/* Number of function imports */
	uint16_t num_syms_vars;			/* Number of variable imports */
	uint16_t num_syms_tls_vars;     /* Number of TLS variable imports */

	uint32_t reserved1;
	uint32_t library_nid;			/* NID of library to import */
	SCE_PTR(const char *) library_name;	/* Pointer to name of imported library, for debugging */
	uint32_t reserved2;
	SCE_PTR(uint32_t *) func_nid_table;	/* Pointer to array of function NIDs to import */
	SCE_PTR(const void **) func_entry_table;/* Pointer to array of stub functions to fill */
	SCE_PTR(uint32_t *) var_nid_table;	/* Pointer to array of variable NIDs to import */
	SCE_PTR(const void **) var_entry_table;	/* Pointer to array of data pointers to write to */
	SCE_PTR(uint32_t *) tls_var_nid_table; /* Pointer to array of TLS variable NIDs to import */
	SCE_PTR(const void **) tls_var_entry_table; /* Pointer to array of data pointers to write to */
} SCE_TYPE(sce_module_imports);

/* alternative module imports struct with a size of 0x24 */
typedef struct SCE_TYPE(sce_module_imports_short) {
	uint16_t size;				/* Size of this struct, set to 0x24 */
	uint16_t version;			/* Set to 0x1 */
	uint16_t flags;				/* Set to 0x0 */
	uint16_t num_syms_funcs;		/* Number of function imports */
	uint16_t num_syms_vars;			/* Number of variable imports */
	uint16_t num_syms_tls_vars;		/* Number of TLS variable imports */
 
	uint32_t library_nid;				/* NID of library to import */
	SCE_PTR(const char *) library_name;	/* Pointer to name of imported library, for debugging */
	SCE_PTR(uint32_t *) func_nid_table;	/* Pointer to array of function NIDs to import */
	SCE_PTR(const void **) func_entry_table;	/* Pointer to array of stub functions to fill */
	SCE_PTR(uint32_t *) var_nid_table;			/* Pointer to array of variable NIDs to import */
	SCE_PTR(const void **) var_entry_table;		/* Pointer to array of data pointers to write to */
} SCE_TYPE(sce_module_imports_short);

#undef SCE_TYPE
#undef SCE_PTR
