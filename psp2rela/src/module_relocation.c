/*
 * PS Vita kernel module manager RE
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <string.h>
#include "debug.h"
#include "module_load_types.h"
#include "modulemgr_internal.h"
#include "module_relocation.h"
#include "rela/module_relocation_types.h"

int module_relocation_write(
		uint32_t *dst,
		int       arm_rel_type,
		uint32_t  offset_rel_target,
		uintptr_t segment_src,
		uint32_t  offset_symbol,
		uintptr_t segment_dst
	){

	int res = 0;
	uint32_t val, tmp;

	switch(arm_rel_type){
	case R_ARM_NONE:
	case R_ARM_V4BX:
		break;
	case R_ARM_ABS32:
	case R_ARM_TARGET1:
		*dst = segment_src + offset_symbol;
		break;
	case R_ARM_REL32:
	case R_ARM_TARGET2:
		*dst = (segment_src - (offset_rel_target + segment_dst)) + offset_symbol;
		break;
	case R_ARM_THM_CALL:
	/*
	 * blx
	 * bl
	 */
		offset_symbol = (segment_src - (offset_rel_target + segment_dst)) + offset_symbol;

		val = (offset_symbol >> 0x18);
		tmp = (offset_symbol >> 1) & 0x7FF;

		if((*dst & 0x10000000) == 0)
			tmp &= ~1;

		*dst = (*dst & 0xD000F800)
		     | (((val ^ ~(offset_symbol >> 0x17)) & 1) << 0x1D)
		     | (((val ^ ~(offset_symbol >> 0x16)) & 1) << 0x1B)
		     | (tmp << 0x10)
		     | ((val & 1) << 0xA)
		     | ((offset_symbol >> 0xC) & 0x3FF);

		break;
	case R_ARM_CALL:
	case R_ARM_JUMP24:
		val = *dst;
		offset_symbol = (segment_src - (offset_rel_target + segment_dst)) + offset_symbol;
		if((val & 0xf0000000) == 0)
			val = (val & 0xfe7fffff) | ((offset_symbol & 3) << 0x17);

		*dst = (val & 0xff000000) | (offset_symbol >> 2 & 0xffffff);
		break;
	case R_ARM_PREL31:
		*dst = (((segment_src - (offset_rel_target + segment_dst)) + offset_symbol) & 0x7fffffff) | (*dst & 80000000);
		break;
	case R_ARM_MOVW_ABS_NC:
		*dst = (*dst & 0xFFF0F000) | ((segment_src + offset_symbol) & 0xFFF) | (((segment_src + offset_symbol) << 4) & 0xF0000);
		break;
	case R_ARM_MOVT_ABS:
		*dst = (*dst & 0xFFF0F000) | ((segment_src + offset_symbol) >> 0x10 & 0xFFF) | (((segment_src + offset_symbol) >> 0x1C) << 0x10);
		break;
	case R_ARM_THM_MOVW_ABS_NC:

		if((*dst & 0x8000FBF0) != 0x0000F240){
			printf_e("illegal movw thumb\n");
			*(int *)(~3) = 0;
		}

		offset_symbol = segment_src + offset_symbol;
		*dst = (*dst & 0x8F00FBF0) | ((offset_symbol & 0xFF) << 16) | ((offset_symbol & 0x700) << 0x14) | (((offset_symbol >> 0xB) & 1) << 10) | ((offset_symbol >> 0xC) & 0xF);
		break;
	case R_ARM_THM_MOVT_ABS:

		if((*dst & 0x8000FBF0) != 0x0000F2C0){
			printf_e("illegal movt thumb\n");
			*(int *)(~3) = 0;
		}

		offset_symbol = segment_src + offset_symbol;
		*dst = (*dst & 0x8F00FBF0) | (offset_symbol & 0xFF0000) | ((offset_symbol >> 0x18 & 7) << 0x1C) | ((offset_symbol >> 0x11) & 0x400) | (offset_symbol >> 0x1C);
		break;
	default:
		res = 0x8002D019;
		break;
	}

	return res;
}

typedef struct SceModuleRelaCtx { // size is 0x14
	uint32_t base;
	uint32_t vaddr;
	SceSize filesz;
	SceSize memsz;
	void *pKernelMap;
} SceModuleRelaCtx;

int sceKernelModuleRelocation(SceModuleLoadCtx *pCtx, const SceRelInfo *rel_info, SceSize rel_info_size){

	SceModuleRelaCtx ctx[4];
	memset(&ctx, 0, sizeof(ctx));

	for(int i=0;i<pCtx->pModuleInfo->segments_num;i++){
		ctx[i].base       = pCtx->segments[i].base;
		ctx[i].vaddr      = pCtx->pModuleInfo->segments[i].vaddr;
		ctx[i].filesz     = pCtx->pModuleInfo->segments[i].filesz;
		ctx[i].memsz      = pCtx->pModuleInfo->segments[i].memsz;
		ctx[i].pKernelMap = pCtx->segments[i].pKernelMap;
	}

	const SceRelInfo *_rel_info     = rel_info;
	const SceRelInfo *_rel_info_end = (const SceRelInfo *)(((uintptr_t)rel_info) + rel_info_size);

	if(rel_info_size == 0)
		return 0;

	if(rel_info == NULL || _rel_info->type >= 2)
		return 0x8002D019;

	int rel_next_size = 0;
	void *current_seg_dst;
	uint32_t rel_type1, rel_type2;
	uint32_t offset_target, offset_symbol;
	uint32_t current_vaddr_dst, current_vaddr_src;
	uint32_t current_seg_target = 0;
	uint32_t current_seg_memsz  = 0;

	rel_type1          = R_ARM_NONE;
	rel_type2          = R_ARM_NONE;
	offset_target      = 0;
	offset_symbol      = 0;
	current_seg_target = _rel_info->type0.r_segment_target;
	current_vaddr_dst  = (uintptr_t)ctx[current_seg_target].vaddr;
	current_vaddr_src  = 0;
	current_seg_dst    = ctx[current_seg_target].pKernelMap;
	current_seg_memsz  = ctx[current_seg_target].memsz;

	uint32_t offsets789, bit_shift;

	while(_rel_info != _rel_info_end){

		switch(_rel_info->type){
		case 0:
			offset_target = _rel_info->type0.r_target_offset;
			offset_symbol = _rel_info->type0.r_symbol_offset;

			rel_type1 = _rel_info->type0.r_code;
			rel_type2 = _rel_info->type0.r_code2;

			if(current_seg_target != _rel_info->type0.r_segment_target){
				current_seg_target = _rel_info->type0.r_segment_target;
				current_vaddr_dst  = (uintptr_t)ctx[current_seg_target].vaddr;
				current_seg_dst    = ctx[current_seg_target].pKernelMap;
				current_seg_memsz  = ctx[current_seg_target].memsz;
			}

			current_vaddr_src = (uintptr_t)ctx[_rel_info->type0.r_segment_symbol].vaddr;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 0);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			if(_rel_info->type0.r_code2 != R_ARM_NONE){
				if((current_seg_memsz - 4) < (offset_target + _rel_info->type0.r_append_offset)){
					printf_e("type%d, target offset overflow\n", 0);
					return 0x8002D019;
				}

				module_relocation_write(
					current_seg_dst + offset_target + _rel_info->type0.r_append_offset, rel_type2,
					offset_target + _rel_info->type0.r_append_offset, current_vaddr_src,
					offset_symbol, current_vaddr_dst
				);
			}

			rel_next_size = sizeof(SceRelInfoType0);

			break;
		case 1:
			offset_target = (_rel_info->type1.r_target_offset_hi << 0xC) | _rel_info->type1.r_target_offset_lo;
			offset_symbol = _rel_info->type1.r_symbol_offset;

			rel_type1 = _rel_info->type1.r_code;
			rel_type2 = R_ARM_NONE;

			if(current_seg_target != _rel_info->type1.r_segment_target){
				current_seg_target = _rel_info->type1.r_segment_target;
				current_vaddr_dst  = (uintptr_t)ctx[current_seg_target].vaddr;
				current_seg_dst    = ctx[current_seg_target].pKernelMap;
				current_seg_memsz  = ctx[current_seg_target].memsz;
			}

			current_vaddr_src = (uintptr_t)ctx[_rel_info->type1.r_segment_symbol].vaddr;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 1);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			rel_next_size = sizeof(SceRelInfoType1);

			break;

		case 2:
			offset_target += _rel_info->type2.r_target_offset;
			offset_symbol  = _rel_info->type2.r_symbol_offset;

			rel_type1 = _rel_info->type2.r_code;
			rel_type2 = R_ARM_NONE;

			current_vaddr_src = (uintptr_t)ctx[_rel_info->type2.r_segment_symbol].vaddr;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 2);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			rel_next_size = sizeof(SceRelInfoType2);
			break;

		case 3:
			offset_target += _rel_info->type3.r_target_offset;
			offset_symbol  = _rel_info->type3.r_symbol_offset;

			if(_rel_info->type3.r_is_thumb != 0){
				rel_type1 = R_ARM_THM_MOVW_ABS_NC;
				rel_type2 = R_ARM_THM_MOVT_ABS;
			}else{
				rel_type1 = R_ARM_MOVW_ABS_NC;
				rel_type2 = R_ARM_MOVT_ABS;
			}

			current_vaddr_src = (uintptr_t)ctx[_rel_info->type3.r_segment_symbol].vaddr;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 3);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			if((current_seg_memsz - 4) < (offset_target + _rel_info->type3.r_append_offset)){
				printf_e("type%d, target offset overflow\n", 3);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target + _rel_info->type3.r_append_offset, rel_type2,
				offset_target + _rel_info->type3.r_append_offset, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			rel_next_size = sizeof(SceRelInfoType3);

			break;
		case 4:
			if(
				(rel_type1 != R_ARM_MOVW_ABS_NC || rel_type2 != R_ARM_MOVT_ABS) &&
				(rel_type1 != R_ARM_THM_MOVW_ABS_NC || rel_type2 != R_ARM_THM_MOVT_ABS)
			){
				printf_e("type%d, invalid rel type\n", 4);
				return 0x8002D019;
			}

			offset_target += _rel_info->type4.r_target_offset;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 4);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			if((current_seg_memsz - 4) < (offset_target + _rel_info->type4.r_append_offset)){
				printf_e("type%d, target offset overflow\n", 4);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target + _rel_info->type4.r_append_offset, rel_type2,
				offset_target + _rel_info->type4.r_append_offset, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			rel_next_size = sizeof(SceRelInfoType4);
			break;

		case 5:
			if(
				(rel_type1 != R_ARM_MOVW_ABS_NC || rel_type2 != R_ARM_MOVT_ABS) &&
				(rel_type1 != R_ARM_THM_MOVW_ABS_NC || rel_type2 != R_ARM_THM_MOVT_ABS)
			){
				printf_e("type%d, invalid rel type\n", 5);
				return 0x8002D019;
			}

			offset_target += _rel_info->type5.r_target_offset1;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 5);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			if((current_seg_memsz - 4) < (offset_target + _rel_info->type5.r_append_offset1)){
				printf_e("type%d, target offset overflow\n", 5);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target + _rel_info->type5.r_append_offset1, rel_type2,
				offset_target + _rel_info->type5.r_append_offset1, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			offset_target += _rel_info->type5.r_target_offset2;

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target offset overflow\n", 5);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target, rel_type1,
				offset_target, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			if((current_seg_memsz - 4) < (offset_target + _rel_info->type5.r_append_offset2)){
				printf_e("type%d, target offset overflow\n", 5);
				return 0x8002D019;
			}

			module_relocation_write(
				current_seg_dst + offset_target + _rel_info->type5.r_append_offset2, rel_type2,
				offset_target + _rel_info->type5.r_append_offset2, current_vaddr_src,
				offset_symbol, current_vaddr_dst
			);

			rel_next_size = sizeof(SceRelInfoType5);
			break;

		case 6:
			if(_rel_info->type6.r_target_offset == 0){
				printf_e("type%d, invalid offset\n", 6);
				return 0x8002D019;
			}

			offset_target += _rel_info->type6.r_target_offset;

			uint32_t new_addr = 0;

			for(int i=0;i<pCtx->pModuleInfo->segments_num;i++){
				if((*(uint32_t *)(current_seg_dst + offset_target) - ctx[i].base) < ctx[i].memsz){
					new_addr = ctx[i].vaddr + (*(uint32_t *)(current_seg_dst + offset_target) - ctx[i].base);
					break;
				}
			}

			if(new_addr == 0){
				printf_e("type%d, address not found. offset_target=0x%08X\n", 6, offset_target);
				return 0x8002D019;
			}

			if((current_seg_memsz - 4) < offset_target){
				printf_e("type%d, target address overflow\n", 6);
				return 0x8002D019;
			}

			*(uint32_t *)(current_seg_dst + offset_target) = new_addr;

			rel_type1 = R_ARM_ABS32;
			rel_type2 = R_ARM_NONE;

			rel_next_size = sizeof(SceRelInfoType6);
			break;

		case 7:
		case 8:
		case 9:
			offsets789 = _rel_info->type789.r_target_offset;

			bit_shift = ((0x1E2 >> ((_rel_info->type * -3) + 0x1B)) & 7);

			do {
				offset_target += ((offsets789 & ((1 << bit_shift) - 1)) * sizeof(uint32_t));

				uint32_t new_addr = 0;

				for(int i=0;i<pCtx->pModuleInfo->segments_num;i++){
					if((*(uint32_t *)(current_seg_dst + offset_target) - ctx[i].base) < ctx[i].memsz){
						new_addr = ctx[i].vaddr + (*(uint32_t *)(current_seg_dst + offset_target) - ctx[i].base);
						break;
					}
				}

				if(new_addr == 0 || (current_seg_memsz - 4) < offset_target){
					printf_e("type%d, address not found or target offset overflow\n", _rel_info->type);
					return 0x8002D019;
				}

				*(uint32_t *)(current_seg_dst + offset_target) = new_addr;

			} while (offsets789 >>= bit_shift);

			rel_type1 = R_ARM_ABS32;
			rel_type2 = R_ARM_NONE;

			rel_next_size = sizeof(SceRelInfoType789);
			break;
		default:
			printf_e("unknown code:0x%X\n", _rel_info->type);
			*(int *)(~1) = 0;
		}

		_rel_info = (const SceRelInfo *)(((uintptr_t)_rel_info) + rel_next_size);
	}

	return 0;
}
