/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include "debug.h"
#include "module_loader.h"
#include "module_relocation.h"
#include "rela_config.h"
#include "rela/convert.h"
#include "rela/core.h"
#include "rela/register.h"
#include "rela/data_register.h"
#include "rela/module_relocation_types.h"

const char *segment_area[] = {"text", "data"};

int rela_data_convert_helper(
	uint32_t *pChecksum, uint32_t segment,
	const void *rel_config0, int rel_config_size0,
	const void *rel_config1, int rel_config_size1,
	void **rel_config_res, int *rel_config_size_res
	){

	int res;

	res = rela_regiser_entrys(rel_config0, rel_config_size0, segment); // Register to split the merged config of vitasdk
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", segment_area[segment], 0);
		return res;
	}

	res = rela_regiser_entrys(rel_config1, rel_config_size1, segment); // Register to split the merged config of vitasdk
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", segment_area[segment], 1);
		return res;
	}

	rela_data_sort_all();
	rela_data_sort_symbol_by_target_address();

	rela_data_calc_checksum(pChecksum);

	res = rela_data_register_open();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_open", segment_area[segment], 0);
		return res;
	}

	res = rela_data_convert(segment);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_convert", segment_area[segment], 0);
		return res;
	}

	res = rela_data_register_close(rel_config_res, rel_config_size_res);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_close", segment_area[segment], 0);
		return res;
	}

	rela_data_free();

	return 0;
}

const char *find_item(int argc, char *argv[], const char *name){

	for(int i=0;i<argc;i++){
		if(strstr(argv[i], name) != NULL){
			return (char *)(strstr(argv[i], name) + strlen(name));
		}
	}

	return NULL;
}

int rela_do_relocation(ModuleLoaderContext *pContext){

	int res, seg0_idx, seg1_idx, seg0_rel_idx, seg1_rel_idx;

	SceModuleLoadCtx moduleLoadCtx;
	SceModuleInfoInternal moduleInfoInternal;
	memset(&moduleLoadCtx, 0, sizeof(moduleLoadCtx));
	memset(&moduleInfoInternal, 0, sizeof(moduleInfoInternal));

	seg0_idx = module_loader_search_elf_index(pContext, 1, 5);
	seg1_idx = module_loader_search_elf_index(pContext, 1, 6);

	seg0_rel_idx = module_loader_search_elf_index(pContext, 0x60000000, 0);
	seg1_rel_idx = module_loader_search_elf_index(pContext, 0x60000000, 0x10000);

	moduleLoadCtx.pModuleInfo = &moduleInfoInternal;

	printf_d("seg0_idx:0x%08X\n", seg0_idx);
	printf_d("seg1_idx:0x%08X\n", seg1_idx);
	printf_d("seg0_rel_idx:0x%08X\n", seg0_rel_idx);
	printf_d("seg1_rel_idx:0x%08X\n", seg1_rel_idx);

	if(seg0_idx >= 0){
		int segment_num = moduleInfoInternal.segments_num;

		uint32_t vaddr = 0x81000000;
		pContext->pPhdr[seg0_idx].p_vaddr = vaddr;
		pContext->pPhdr[seg0_idx].p_paddr = vaddr;
		moduleInfoInternal.segments[segment_num].vaddr = vaddr;
		moduleInfoInternal.segments[segment_num].memsz  = pContext->pPhdr[seg0_idx].p_memsz;
		moduleInfoInternal.segments[segment_num].filesz = pContext->pPhdr[seg0_idx].p_filesz;
		moduleLoadCtx.segments[segment_num].base = pContext->pPhdr[seg0_idx].p_vaddr;
		moduleLoadCtx.segments[segment_num].pKernelMap = pContext->segment[seg0_idx].pData;

		moduleInfoInternal.segments_num = segment_num + 1;

		printf_i("segment %d new vaddr : 0x%08X(0x%08X)\n", segment_num, vaddr, moduleInfoInternal.segments[segment_num].memsz);
	}

	if(seg1_idx >= 0){
		int segment_num = moduleInfoInternal.segments_num;

		uint32_t vaddr = ((0x81000000 + pContext->pPhdr[seg0_idx].p_memsz) + 0xFFF) & ~0xFFF;
		pContext->pPhdr[seg1_idx].p_vaddr = vaddr;
		pContext->pPhdr[seg1_idx].p_paddr = vaddr;
		moduleInfoInternal.segments[segment_num].vaddr = vaddr;
		moduleInfoInternal.segments[segment_num].memsz  = pContext->pPhdr[seg1_idx].p_memsz;
		moduleInfoInternal.segments[segment_num].filesz = pContext->pPhdr[seg1_idx].p_filesz;
		moduleLoadCtx.segments[segment_num].base = pContext->pPhdr[seg1_idx].p_vaddr;
		moduleLoadCtx.segments[segment_num].pKernelMap = pContext->segment[seg1_idx].pData;

		moduleInfoInternal.segments_num = segment_num + 1;

		printf_i("segment %d new vaddr : 0x%08X(0x%08X)\n", segment_num, vaddr, moduleInfoInternal.segments[segment_num].memsz);
	}

	printf_i("\n");

	res = 0;

	if(res == 0 && seg0_rel_idx >= 0)
		res = sceKernelModuleRelocation(&moduleLoadCtx, pContext->segment[seg0_rel_idx].pData, pContext->pPhdr[seg0_rel_idx].p_filesz);

	if(res == 0 && seg1_rel_idx >= 0)
		res = sceKernelModuleRelocation(&moduleLoadCtx, pContext->segment[seg1_rel_idx].pData, pContext->pPhdr[seg1_rel_idx].p_filesz);

	return res;
}

int main(int argc, char *argv[]){

	int res;
	ModuleLoaderContext *pContext;

	const char *src_path = find_item(argc, argv, "-src=");
	const char *dst_path = find_item(argc, argv, "-dst=");

	if(argc == 1 || src_path == NULL){
		printf("psp2rela -src=in_file [-dst=out_file] [-flag=any_flags] [-log_dst=log_path]\n");
		return 1;
	}

	const char *log_flag = find_item(argc, argv, "-flag=");
	const char *log_path = find_item(argc, argv, "-log_dst=");

	rela_debug_init(log_flag, log_path);

	printf_i("src=%s\n", src_path);
	if(dst_path != NULL)
		printf_i("dst=%s\n", dst_path);

	printf_i("\n");

	res = module_loader_open(src_path, &pContext);
	if(res < 0){
		printf_e("cannot open module\n");
		goto error;
	}

	printf_d("module open success\n");

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){

#if defined(RELA_USE_DEFAULT_ALIGN_SIZE) && RELA_USE_DEFAULT_ALIGN_SIZE != 0
		if(pContext->pPhdr[i].p_align == 0x1000)
			pContext->pPhdr[i].p_align = 0x10;
#endif
		if(module_loader_is_elf(pContext) == 0 && pContext->pSegmentInfo[i].compression == 2 && pContext->pPhdr[i].p_filesz != 0){

			long unsigned int temp_size = pContext->pPhdr[i].p_filesz;
			void *temp_memory_ptr = malloc(temp_size);

			res = uncompress(temp_memory_ptr, &temp_size, pContext->segment[i].pData, pContext->pSegmentInfo[i].length);
			if(res != Z_OK){
				printf_e("zlib uncompress failed : 0x%X\n", res);
				printf_e("\telf segment %d\n", i);
				printf_e("\tsegment file size 0x%X\n", pContext->pPhdr[i].p_filesz);
				free(temp_memory_ptr);
				goto error;
			}

			free(pContext->segment[i].pData);
			pContext->segment[i].pData = temp_memory_ptr;
		}
	}

#if defined(RELA_PRE_RELOCATION) && RELA_PRE_RELOCATION != 0

	printf_d("Pre-relocation ...\n");

	res = rela_do_relocation(pContext);
	if(res < 0){
		printf_e("rela_do_relocation failed : 0x%X\n", res);
		goto error;
	}

	printf_d("Pre-relocation ... ok\n\n");
#endif

	void *rel_config0 = NULL, *rel_config1 = NULL, *rel_config0_res = NULL, *rel_config1_res = NULL;
	long unsigned int rel_config_size0 = 0, rel_config_size1 = 0;
	int rel_config_size0_res = 0, rel_config_size1_res = 0;

	int seg0_rel_idx, seg1_rel_idx;

	seg0_rel_idx = module_loader_search_elf_index(pContext, 0x60000000, 0);
	seg1_rel_idx = module_loader_search_elf_index(pContext, 0x60000000, 0x10000);
	if(seg0_rel_idx < 0){
		printf_e("cannot get text segment rel config index\n");
		goto error;
	}

	rel_config0      = pContext->segment[seg0_rel_idx].pData;
	rel_config_size0 = pContext->pPhdr[seg0_rel_idx].p_filesz;
	if(seg1_rel_idx >= 0){
		rel_config1      = pContext->segment[seg1_rel_idx].pData;
		rel_config_size1 = pContext->pPhdr[seg1_rel_idx].p_filesz;
	}

	printf_i("Original segment rel config size text=0x%08X data=0x%08X\n\n", rel_config_size0, rel_config_size1);

	if(rela_is_show_mode() != 0){
		rela_regiser_entrys(rel_config0, rel_config_size0, 0);
		rela_regiser_entrys(rel_config1, rel_config_size1, 0); // Register to split the merged config of vitasdk
		rela_data_sort_all();
		rela_data_sort_symbol_by_target_address();

		printf_i("\n");
		printf_i("Text segment\n\n");

		rela_data_calc_checksum(NULL);
		rela_data_show();
		rela_data_free();

		rela_regiser_entrys(rel_config0, rel_config_size0, 1); // Register to split the merged config of vitasdk
		rela_regiser_entrys(rel_config1, rel_config_size1, 1);
		rela_data_sort_all();
		rela_data_sort_symbol_by_target_address();

		printf_i("\n");
		printf_i("Data segment\n\n");

		rela_data_calc_checksum(NULL);
		rela_data_show();
		rela_data_free();

		goto module_close;
	}

	uint32_t checksum_org0, checksum_org1;

	/*
	 * Convert segment rel config
	 */
	res = rela_data_convert_helper(&checksum_org0, 0, rel_config0, rel_config_size0, rel_config1, rel_config_size1, &rel_config0_res, &rel_config_size0_res);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_convert_helper", "text", 0);
		goto error;
	}

	printf_i("Text segment rel config size : 0x%X\n", rel_config_size0_res);
	printf_i("\n");

	res = rela_data_convert_helper(&checksum_org1, 1, rel_config0, rel_config_size0, rel_config1, rel_config_size1, &rel_config1_res, &rel_config_size1_res);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_convert_helper", "data", 0);
		goto error;
	}

	printf_i("Data segment rel config size : 0x%X\n", rel_config_size1_res);

	/*
	 * Update segment infos
	 */
	if(seg1_rel_idx < 0 && rel_config1_res != NULL)
		seg1_rel_idx = module_loader_add_elf_entry(pContext, 0x60000000, 0x10000);

	if(seg1_rel_idx < 0 && rel_config1_res != NULL){
		printf_e("cannot add elf entry\n");
		goto error;
	}

	free(pContext->segment[seg0_rel_idx].pData);
	pContext->segment[seg0_rel_idx].pData = rel_config0_res;
	pContext->pPhdr[seg0_rel_idx].p_filesz = rel_config_size0_res;

#if defined(RELA_ENABLE_CHECKSUM) && RELA_ENABLE_CHECKSUM != 0

	printf_i("\nsegment %d rel config checksum ...\n", 0);

	rela_regiser_entrys(rel_config0_res, rel_config_size0_res, 0);
	rela_data_sort_all();
	rela_data_sort_symbol_by_target_address();
	rel_config0_res = NULL;
	rel_config_size0_res = 0;

	uint32_t checksum_new0;
	rela_data_calc_checksum(&checksum_new0);

	rela_data_free();

	if(checksum_org0 != checksum_new0){
		printf_e("Segment %d checksum is mismatch = 0x%08X/0x%08X\n", 0, checksum_org0, checksum_new0);
		goto error;
	}

	printf_i("segment %d rel config checksum ... ok\n", 0);
#endif

	if(seg1_rel_idx >= 0){
		free(pContext->segment[seg1_rel_idx].pData);
		pContext->segment[seg1_rel_idx].pData = rel_config1_res;
		pContext->pPhdr[seg1_rel_idx].p_filesz = rel_config_size1_res;

#if defined(RELA_ENABLE_CHECKSUM) && RELA_ENABLE_CHECKSUM != 0

		printf_i("\nsegment %d rel config checksum ...\n", 1);

		rela_regiser_entrys(rel_config1_res, rel_config_size1_res, 1);
		rela_data_sort_all();
		rela_data_sort_symbol_by_target_address();
		rel_config1_res = NULL;
		rel_config_size1_res = 0;

		uint32_t checksum_new1;
		rela_data_calc_checksum(&checksum_new1);

		rela_data_free();

		if(checksum_org1 != checksum_new1){
			printf_e("Segment %d checksum is mismatch = 0x%08X/0x%08X\n", 1, checksum_org1, checksum_new1);
			goto error;
		}

		printf_i("segment %d rel config checksum ... ok\n", 1);
#endif
	}

	rel_config0_res = NULL;
	rel_config1_res = NULL;

	/*
	 * Rebuild segment infos
	 */
	for(int i=0;i<pContext->pEhdr->e_phnum;i++){
		if(module_loader_is_elf(pContext) == 0 && pContext->pPhdr[i].p_filesz != 0){
			long unsigned int rel_config_size0_tmp = (pContext->pPhdr[i].p_filesz << 1) + 12;
			void *rel_config0_tmp = malloc(rel_config_size0_tmp);

			res = compress(rel_config0_tmp, &rel_config_size0_tmp, pContext->segment[i].pData, pContext->pPhdr[i].p_filesz);
			if(res != Z_OK){
				printf_e("zlib compress failed : 0x%X\n", res);
				printf_e("\telf segment %d\n", i);
				printf_e("\tsegment file size 0x%X\n", pContext->pPhdr[i].p_filesz);
				free(rel_config0_tmp);
				goto error;
			}

			free(pContext->segment[i].pData);
			pContext->segment[i].pData = rel_config0_tmp;
			pContext->pSegmentInfo[i].length = rel_config_size0_tmp;
			pContext->pSegmentInfo[i].compression = 2;
		}
	}

	/*
	 * Save re converted module
	 */
	if(dst_path != NULL)
		module_loader_save(pContext, dst_path);

module_close:
	module_loader_close(pContext);
	pContext = NULL;

	rela_debug_fini();

	return 0;

error:
	printf_e("The process was aborted due to an error\n");

	if(rel_config0_res != NULL){
		free(rel_config0_res);
		rel_config0_res = NULL;
	}

	if(rel_config1_res != NULL){
		free(rel_config1_res);
		rel_config1_res = NULL;
	}

	rela_data_register_close(NULL, NULL);
	rela_data_free();

	module_loader_close(pContext);
	pContext = NULL;

	rela_debug_fini();

	return 1;
}
