/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_MODULE_LOADER_H_
#define _PSP2_MODULE_LOADER_H_

#include "psp2/elf.h"
#include "psp2/self.h"

typedef struct ModuleSegmentInfo {
	void *pData;
	uint32_t memsz;
} ModuleSegmentInfo;

typedef struct ModuleLoaderContext {
	int is_elf;
	void *pHeader;
	ext_header *pExtHeader;
	Elf32_Ehdr *pEhdr;
	Elf32_Phdr *pPhdr;
	SCE_appinfo *pAppInfo;
	SCE_version *pVersion;
	segment_info *pSegmentInfo;
	void *pControlInfo;
	ModuleSegmentInfo segment[6];
} ModuleLoaderContext;

int module_loader_open(const char *path, ModuleLoaderContext **ppResult);
int module_loader_close(ModuleLoaderContext *pContext);

int module_loader_save(ModuleLoaderContext *pContext, const char *path);

int module_loader_is_elf(const ModuleLoaderContext *pContext);

int module_loader_search_elf_index(ModuleLoaderContext *pContext, int type, int flags);
int module_loader_add_elf_entry(ModuleLoaderContext *pContext, int type, int flags);

#endif /* _PSP2_MODULE_LOADER_H_ */
