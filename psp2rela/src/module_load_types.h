/*
 * PS Vita kernel module manager RE
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_MODULE_LOAD_TYPE_H_
#define _PSP2_MODULE_LOAD_TYPE_H_

#include "psp2/types.h"
#include "modulemgr_internal.h"

typedef struct SceModuleLoadCtx { // size is 0x44
	SceModuleInfoInternal *pModuleInfo;
	int data_0x04;
	int data_0x08;
	int data_0x0C;
	int data_0x10;
	int data_0x14;
	int data_0x18;
	int data_0x1C;
	struct {
		uint32_t base; // from elf header
		SceUID data_0x24;
		void *pKernelMap;
	} segments[3];
} SceModuleLoadCtx;

#endif /* _PSP2_MODULE_LOAD_TYPE_H_ */
