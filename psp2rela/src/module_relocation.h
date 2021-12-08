/*
 * PS Vita kernel module manager RE
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_MODULE_RELOCATION_H_
#define _PSP2_MODULE_RELOCATION_H_

#include "module_load_types.h"
#include "rela/module_relocation_types.h"

int sceKernelModuleRelocation(SceModuleLoadCtx *pCtx, const SceRelInfo *rel_info, SceSize rel_info_size);

#endif /* _PSP2_MODULE_RELOCATION_H_ */
