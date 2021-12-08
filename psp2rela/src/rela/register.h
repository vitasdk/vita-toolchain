/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_RELA_REGISTER_H_
#define _PSP2_RELA_REGISTER_H_

#include "module_relocation_types.h"

int rela_regiser_entrys(const SceRelInfo *rel_info, unsigned int rel_info_size, uint32_t x_target_segment);

#endif /* _PSP2_RELA_REGISTER_H_ */
