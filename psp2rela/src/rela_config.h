/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_RELA_CONFIG_H_
#define _PSP2_RELA_CONFIG_H_

/*
 * For psp2rela development
 */
#define RELA_ENABLE_CHECKSUM        (1)

/*
 * For self/module rel config optimization
 */
#define RELA_PRE_RELOCATION         (1)

/*
 * For module align 0x1000 to 0x10
 */
#define RELA_USE_DEFAULT_ALIGN_SIZE (1)

/*
 * For rel type 6/7/8/9
 */
#define RELA_USE_ABS32_TYPE         (1)

/*
 * For ABS32 rel config optimization
 */
#define RELA_ABS32_SORT_ENABLE      (1)

#endif /* _PSP2_RELA_CONFIG_H_ */
