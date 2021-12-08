/*
 * PS Vita kernel module manager RE
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_MODULE_RELOCATION_TYPE_H_
#define _PSP2_MODULE_RELOCATION_TYPE_H_

#include <stdint.h>

#define SCE_RELOCATION_BASE0 (0)
#define SCE_RELOCATION_BASE1 (1)
#define SCE_RELOCATION_BASE2 (2)
#define SCE_RELOCATION_MVTW0 (3)
#define SCE_RELOCATION_MVTW1 (4)
#define SCE_RELOCATION_MVTW2 (5)
#define SCE_RELOCATION_WORD0 (6)
#define SCE_RELOCATION_WORD1 (7)
#define SCE_RELOCATION_WORD2 (8)
#define SCE_RELOCATION_WORD3 (9)

#ifndef R_ARM_NONE
#define R_ARM_NONE            (0)
#define R_ARM_ABS32           (2)
#define R_ARM_REL32           (3)
#define R_ARM_THM_CALL        (10)
#define R_ARM_CALL            (28)
#define R_ARM_JUMP24          (29)
#define R_ARM_TARGET1         (38)
#define R_ARM_V4BX            (40)
#define R_ARM_TARGET2         (41)
#define R_ARM_PREL31          (42)
#define R_ARM_MOVW_ABS_NC     (43)
#define R_ARM_MOVT_ABS        (44)
#define R_ARM_THM_MOVW_ABS_NC (47)
#define R_ARM_THM_MOVT_ABS    (48)
#define R_ARM_RBASE           (255)
#endif

typedef struct SceRelInfoType0 {
	uint32_t r_type           : 4;
	uint32_t r_segment_symbol : 4;
	uint32_t r_code           : 8;
	uint32_t r_segment_target : 4;
	uint32_t r_code2          : 7;
	uint32_t r_append_offset  : 5;

	uint32_t r_symbol_offset;
	uint32_t r_target_offset;
} SceRelInfoType0;

typedef struct SceRelInfoType1 {
	uint32_t r_type             : 4;
	uint32_t r_segment_symbol   : 4;
	uint32_t r_code             : 8;
	uint32_t r_segment_target   : 4;
	uint32_t r_target_offset_lo : 12;

	uint32_t r_target_offset_hi : 10;
	uint32_t r_symbol_offset    : 22;
} SceRelInfoType1;

typedef struct SceRelInfoType2 {
	uint32_t r_type           : 4;
	uint32_t r_segment_symbol : 4;
	uint32_t r_code           : 8;
	uint32_t r_target_offset  : 16;

	uint32_t r_symbol_offset;
} SceRelInfoType2;

typedef struct SceRelInfoType3 {
	uint32_t r_type           : 4;
	uint32_t r_segment_symbol : 4;
	uint32_t r_is_thumb       : 1;
	uint32_t r_target_offset  : 18;
	uint32_t r_append_offset  : 5;

	uint32_t r_symbol_offset;
} SceRelInfoType3;

typedef struct SceRelInfoType4 {
	uint32_t r_type          : 4;
	uint32_t r_target_offset : 23;
	uint32_t r_append_offset : 5;
} SceRelInfoType4;

typedef struct SceRelInfoType5 {
	uint32_t r_type           : 4;
	uint32_t r_target_offset1 : 9;
	uint32_t r_append_offset1 : 5;
	uint32_t r_target_offset2 : 9;
	uint32_t r_append_offset2 : 5;
} SceRelInfoType5;

typedef struct SceRelInfoType6 {
	uint32_t r_type          : 4;
	uint32_t r_target_offset : 28;
} SceRelInfoType6;

typedef struct SceRelInfoType789 {
	uint32_t r_type          : 4;
	uint32_t r_target_offset : 28;
} SceRelInfoType789;

typedef union SceRelInfo {
	uint32_t type : 4;
	SceRelInfoType0 type0;
	SceRelInfoType1 type1;
	SceRelInfoType2 type2;
	SceRelInfoType3 type3;
	SceRelInfoType4 type4;
	SceRelInfoType5 type5;
	SceRelInfoType6 type6;
	SceRelInfoType789 type789;
} SceRelInfo;

#endif /* _PSP2_MODULE_RELOCATION_TYPE_H_ */
