/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "data_register.h"
#include "module_relocation_types.h"

#define RELA_DATA_REGISTER_POOL_SIZE (0x4000)
#define RELA_DATA_REGISTER_POOL_ALIGN(length) ((length + (RELA_DATA_REGISTER_POOL_SIZE - 1)) & ~(RELA_DATA_REGISTER_POOL_SIZE - 1))

int rela_data_is_register = 0;
int rela_data_write_size = 0;
void *rela_data_pool = NULL;

int rela_data_register_open(void){

	if(rela_data_is_register != 0)
		return -1;

	rela_data_is_register = 1;

	rela_data_write_size = 0;
	rela_data_pool       = NULL;

	return 0;
}

int rela_data_register_close(void **ppRelaDataResult, int *pResultSize){

	if(rela_data_is_register == 0)
		return -1;

	rela_data_is_register = 0;

	if(ppRelaDataResult == NULL){
		free(rela_data_pool);
	}else{
		*ppRelaDataResult = rela_data_pool;
	}

	if(pResultSize != NULL)
		*pResultSize      = rela_data_write_size;

	rela_data_write_size = 0;
	rela_data_pool       = NULL;

	return 0;
}

int rela_data_register_write(const void *data, int size){

	if(rela_data_is_register == 0)
		return -1;

	if(size >= (RELA_DATA_REGISTER_POOL_SIZE >> 1))
		return -2;

	if((rela_data_write_size + size) >= RELA_DATA_REGISTER_POOL_ALIGN(rela_data_write_size)){

		void *tmp_ptr = malloc(RELA_DATA_REGISTER_POOL_ALIGN(rela_data_write_size + size));
		if(tmp_ptr == NULL)
			return -2;

		if(rela_data_pool != NULL){
			memcpy(tmp_ptr, rela_data_pool, rela_data_write_size);
			free(rela_data_pool);
		}

		rela_data_pool = tmp_ptr;
	}

	memcpy((void *)(((uintptr_t)rela_data_pool) + rela_data_write_size), data, size);

	rela_data_write_size += size;

	return 0;
}

int rela_data_register_write_type0(
	uint32_t symbol_segment, uint32_t symbol_address,
	uint32_t target_segment, uint32_t target_address,
	uint32_t append, uint32_t rel_type0, uint32_t rel_type1
	){

	if(symbol_segment >= 0x10 || target_segment >= 0x10 || append > 0x1E || rel_type0 >= 0x100 || rel_type1 >= 0x100)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType0 rel_info;

	rel_info.r_type           = 0;
	rel_info.r_segment_symbol = symbol_segment;
	rel_info.r_code           = rel_type0;
	rel_info.r_segment_target = target_segment;
	rel_info.r_code2          = rel_type1;
	rel_info.r_append_offset  = append;
	rel_info.r_symbol_offset  = symbol_address;
	rel_info.r_target_offset  = target_address;

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType0));
}

int rela_data_register_write_type1(
	uint32_t symbol_segment, uint32_t symbol_address,
	uint32_t target_segment, uint32_t target_address,
	uint32_t rel_type
	){

	if(
		symbol_segment >= (1 << 4) || symbol_address >= (1 << 22) ||
		target_segment >= (1 << 4) || target_address >= (1 << 22) ||
		rel_type >= (1 << 8)
	)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType1 rel_info;

	rel_info.r_type             = 1;
	rel_info.r_segment_symbol   = symbol_segment;
	rel_info.r_code             = rel_type;
	rel_info.r_segment_target   = target_segment;
	rel_info.r_target_offset_lo = target_address & ((1 << 12) - 1);
	rel_info.r_target_offset_hi = (target_address >> 12) & ((1 << 10) - 1);
	rel_info.r_symbol_offset    = symbol_address & ((1 << 22) - 1);

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType1));
}

int rela_data_register_write_type2(uint32_t symbol_segment, uint32_t symbol_address, uint32_t target_address, uint32_t rel_type){

	if(symbol_segment >= 0x10 || target_address >= (1 << 16))
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType2 rel_info;

	rel_info.r_type           = 2;
	rel_info.r_segment_symbol = symbol_segment;
	rel_info.r_code           = rel_type;
	rel_info.r_target_offset  = target_address & ((1 << 16) - 1);
	rel_info.r_symbol_offset  = symbol_address;

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType2));
}

int rela_data_register_write_type3(uint32_t symbol_segment, uint32_t symbol_address, uint32_t target_address, uint32_t append_offset, uint32_t is_thumb){

	if(target_address >= (1 << 18) || append_offset >= (1 << 5) || is_thumb > 1)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType3 rel_info;

	rel_info.r_type           = 3;
	rel_info.r_segment_symbol = symbol_segment;
	rel_info.r_is_thumb       = is_thumb;
	rel_info.r_target_offset  = target_address;
	rel_info.r_append_offset  = append_offset;
	rel_info.r_symbol_offset  = symbol_address;

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType3));
}

int rela_data_register_write_type4(uint32_t target_address, uint32_t append_offset){

	if(target_address >= 0x10000000 || append_offset > 0x1E)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType4 rel_info;

	rel_info.r_type          = 4;
	rel_info.r_target_offset = target_address;
	rel_info.r_append_offset = append_offset;

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType4));
}

int rela_data_register_write_type5(uint32_t target_offset1, uint32_t append_offset1, uint32_t target_offset2, uint32_t append_offset2){

	if(
		target_offset1 >= (1 << 9) || append_offset1 >= (1 << 5) ||
		target_offset2 >= (1 << 9) || append_offset2 >= (1 << 5)
	)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType5 rel_info;

	rel_info.r_type           = 5;
	rel_info.r_target_offset1 = target_offset1;
	rel_info.r_append_offset1 = append_offset1;
	rel_info.r_target_offset2 = target_offset2;
	rel_info.r_append_offset2 = append_offset2;

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType5));
}

int rela_data_register_write_type6(uint32_t target_address){

	if(target_address >= (1 << 28))
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType6 rel_info;

	rel_info.r_type          = 6;
	rel_info.r_target_offset = target_address & ((1 << 28) - 1);

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType6));
}

int rela_data_register_write_type7(uint32_t target_address){

	if(target_address >= 0x10000000)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType789 rel_info;

	rel_info.r_type          = 7;
	rel_info.r_target_offset = target_address & ((1 << 28) - 1);

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType789));
}

int rela_data_register_write_type8(uint32_t target_address){

	if(target_address >= 0x10000000)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType789 rel_info;

	rel_info.r_type          = 8;
	rel_info.r_target_offset = target_address & ((1 << 28) - 1);

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType789));
}

int rela_data_register_write_type9(uint32_t target_address){

	if(target_address >= 0x10000000)
		return RELA_ERROR_DATA_REGISTER_INVALID_PARAM;

	SceRelInfoType789 rel_info;

	rel_info.r_type          = 9;
	rel_info.r_target_offset = target_address & ((1 << 28) - 1);

	return rela_data_register_write(&rel_info, sizeof(SceRelInfoType789));
}
