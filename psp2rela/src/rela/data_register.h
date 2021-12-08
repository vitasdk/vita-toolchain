/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_RELA_DATA_REGISTER_H_
#define _PSP2_RELA_DATA_REGISTER_H_

#define RELA_ERROR_DATA_REGISTER_INVALID_PARAM (0x80040001)

int rela_data_register_open(void);
int rela_data_register_close(void **ppRelaDataResult, int *pResultSize);

int rela_data_register_write(const void *data, int size);

int rela_data_register_write_type0(
	uint32_t symbol_segment, uint32_t symbol_address,
	uint32_t target_segment, uint32_t target_address,
	uint32_t append, uint32_t rel_type0, uint32_t rel_type1
);
int rela_data_register_write_type1(uint32_t symbol_segment, uint32_t symbol_address, uint32_t target_segment, uint32_t target_address, uint32_t rel_type);
int rela_data_register_write_type2(uint32_t symbol_segment, uint32_t symbol_address, uint32_t target_address, uint32_t rel_type);
int rela_data_register_write_type3(uint32_t symbol_segment, uint32_t symbol_address, uint32_t target_address, uint32_t append_offset, uint32_t is_thumb);
int rela_data_register_write_type4(uint32_t target_address, uint32_t append_offset);
int rela_data_register_write_type5(uint32_t target_offset1, uint32_t append_offset1, uint32_t target_offset2, uint32_t append_offset2);
int rela_data_register_write_type6(uint32_t target_address);
int rela_data_register_write_type7(uint32_t target_address);
int rela_data_register_write_type8(uint32_t target_address);
int rela_data_register_write_type9(uint32_t target_address);

#endif /* _PSP2_RELA_DATA_REGISTER_H_ */
