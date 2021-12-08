/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "core.h"
#include "register.h"
#include "module_relocation_types.h"
#include "../debug.h"

int rela_regiser_entrys(const SceRelInfo *rel_info, unsigned int rel_info_size, uint32_t x_target_segment){

	const SceRelInfo *_rel_info     = rel_info;
	const SceRelInfo *_rel_info_end = (const SceRelInfo *)(((uintptr_t)rel_info) + rel_info_size);

	rela_data_set_registered_num(0);

	if(rel_info_size == 0){
		printf_i("%d:No rel info. skip register\n", x_target_segment);
		return 0;
	}

	if(_rel_info->type >= 2){
		printf_e("%d:First rel info is not type 0 or 1 (first=%d). return 0x%X\n", x_target_segment, _rel_info->type, 0x8002D019);
		return 0x8002D019;
	}

	int rel_next_size = 0;
	void *current_seg_dst;
	uint32_t rel_type1, rel_type2;
	uint32_t offset_target, offset_symbol;
	uintptr_t current_vaddr_dst, current_vaddr_src;
	uint32_t current_seg_target = 0;
	uint32_t current_seg_memsz  = 0;

	rel_type1          = R_ARM_NONE;
	rel_type2          = R_ARM_NONE;
	offset_target      = 0;
	offset_symbol      = 0;

	uint32_t offsets789, bit_shift;

	int sym_seg, count = 0, entry_num = 0;

	while(_rel_info != _rel_info_end){

		count++;

		switch(_rel_info->type){
		case 0:
			printf_t("type%d\n", _rel_info->type);

			offset_target = _rel_info->type0.r_target_offset;
			offset_symbol = _rel_info->type0.r_symbol_offset;

			rel_type1 = _rel_info->type0.r_code;
			rel_type2 = _rel_info->type0.r_code2;

			current_seg_target = _rel_info->type0.r_segment_target;
			sym_seg = _rel_info->type0.r_segment_symbol;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);
			rela_data_add_entry(current_seg_target, offset_target + _rel_info->type0.r_append_offset, sym_seg, offset_symbol, rel_type2, x_target_segment);

			entry_num += ((rel_type2 != R_ARM_NONE) ? 2 : 1);

			rel_next_size = sizeof(SceRelInfoType0);

			break;

		case 1:
			printf_t("type%d\n", _rel_info->type);

			offset_target = (_rel_info->type1.r_target_offset_hi << 0xC) | _rel_info->type1.r_target_offset_lo;
			offset_symbol = _rel_info->type1.r_symbol_offset;

			rel_type1 = _rel_info->type1.r_code;
			rel_type2 = R_ARM_NONE;

			current_seg_target = _rel_info->type1.r_segment_target;
			sym_seg = _rel_info->type1.r_segment_symbol;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);

			entry_num += 1;

			rel_next_size = sizeof(SceRelInfoType1);

			break;

		case 2:
			printf_t("type%d\n", _rel_info->type);

			offset_target += _rel_info->type2.r_target_offset;
			offset_symbol  = _rel_info->type2.r_symbol_offset;

			rel_type1 = _rel_info->type2.r_code;
			rel_type2 = R_ARM_NONE;

			sym_seg = _rel_info->type2.r_segment_symbol;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);

			entry_num += 1;

			rel_next_size = sizeof(SceRelInfoType2);
			break;

		case 3:
			printf_t("type%d\n", _rel_info->type);

			offset_target += _rel_info->type3.r_target_offset;
			offset_symbol  = _rel_info->type3.r_symbol_offset;

			if(_rel_info->type3.r_is_thumb != 0){
				rel_type1 = R_ARM_THM_MOVW_ABS_NC;
				rel_type2 = R_ARM_THM_MOVT_ABS;
			}else{
				rel_type1 = R_ARM_MOVW_ABS_NC;
				rel_type2 = R_ARM_MOVT_ABS;
			}

			sym_seg = _rel_info->type3.r_segment_symbol;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);
			rela_data_add_entry(current_seg_target, offset_target + _rel_info->type3.r_append_offset, sym_seg, offset_symbol, rel_type2, x_target_segment);

			entry_num += 2;

			rel_next_size = sizeof(SceRelInfoType3);

			break;

		case 4:
			printf_t("type%d\n", _rel_info->type);

			offset_target += _rel_info->type4.r_target_offset;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);
			rela_data_add_entry(current_seg_target, offset_target + _rel_info->type4.r_append_offset, sym_seg, offset_symbol, rel_type2, x_target_segment);

			entry_num += 2;

			rel_next_size = sizeof(SceRelInfoType4);
			break;

		case 5:
			printf_t("type%d\n", _rel_info->type);

			offset_target += _rel_info->type5.r_target_offset1;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);
			rela_data_add_entry(current_seg_target, offset_target + _rel_info->type5.r_append_offset1, sym_seg, offset_symbol, rel_type2, x_target_segment);

			offset_target += _rel_info->type5.r_target_offset2;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, rel_type1, x_target_segment);
			rela_data_add_entry(current_seg_target, offset_target + _rel_info->type5.r_append_offset2, sym_seg, offset_symbol, rel_type2, x_target_segment);

			entry_num += 4;

			rel_next_size = sizeof(SceRelInfoType5);
			break;

		case 6:
			if(_rel_info->type6.r_target_offset == 0)
				return 0x8002D019;

			printf_t("type%d\n", _rel_info->type);

			offset_target += _rel_info->type6.r_target_offset;

			rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, R_ARM_ABS32, x_target_segment);

			rel_type1 = R_ARM_ABS32;
			rel_type2 = R_ARM_NONE;

			entry_num += 1;

			rel_next_size = sizeof(SceRelInfoType6);
			break;

		case 7:
		case 8:
		case 9:
			printf_t("type%d\n", _rel_info->type);
			offsets789 = _rel_info->type789.r_target_offset;

			/*
			 * 7:7
			 * 8:4
			 * 9:2
			 */
			bit_shift = ((0x1E2 >> ((_rel_info->type * -3) + 0x1B)) & 7);

			do {
				offset_target += ((offsets789 & ((1 << bit_shift) - 1)) * sizeof(uint32_t));

				rela_data_add_entry(current_seg_target, offset_target, sym_seg, offset_symbol, R_ARM_ABS32, x_target_segment);

				entry_num += 1;
			} while(offsets789 >>= bit_shift);

			rel_type1 = R_ARM_ABS32;
			rel_type2 = R_ARM_NONE;

			rel_next_size = sizeof(SceRelInfoType789);
			break;
		default:
			printf_e("unknown code:0x%X\n", _rel_info->type);
			return -1;
		}

		_rel_info = (const SceRelInfo *)(((uintptr_t)_rel_info) + rel_next_size);
	}

	printf_i("%d:%d infos, %d entrys (with NONE code)\n", x_target_segment, count, entry_num);
	printf_i("%d:%d entrys (registered)\n", x_target_segment, rela_data_get_registered_num());

	return 0;
}
