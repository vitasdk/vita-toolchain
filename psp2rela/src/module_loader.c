/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "debug.h"
#include "module_loader.h"

int module_loader_close(ModuleLoaderContext *pContext){

	if(pContext == NULL){
		return -1;
	}

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){
		if(pContext->segment[i].pData != NULL)
			free(pContext->segment[i].pData);
	}

	free(pContext->pSegmentInfo);
	free(pContext->pAppInfo);
	free(pContext->pVersion);
	free(pContext->pControlInfo);
	free(pContext->pPhdr);
	free(pContext->pHeader);
	free(pContext);

	return 0;
}

int module_loader_search_elf_index(ModuleLoaderContext *pContext, int type, int flags){

	if(pContext == NULL){
		return -1;
	}

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){
		if(pContext->pPhdr[i].p_type == type && pContext->pPhdr[i].p_flags == flags)
			return i;
	}

	return -2;
}

int module_loader_add_elf_entry(ModuleLoaderContext *pContext, int type, int flags){

	int res;

	if(pContext == NULL)
		return -1;

	res = module_loader_search_elf_index(pContext, type, flags);
	if(res >= 0)
		return -4;

	int i = pContext->pEhdr->e_phnum;

	Elf32_Phdr *pPhdrTmp = (Elf32_Phdr *)malloc(((sizeof(Elf32_Phdr) * (i + 1)) + 0xF) & ~0xF);

	memset(pPhdrTmp, 0, ((sizeof(Elf32_Phdr) * (i + 1)) + 0xF) & ~0xF);
	memcpy(pPhdrTmp, pContext->pPhdr, sizeof(Elf32_Phdr) * i);
	free(pContext->pPhdr);
	pContext->pPhdr = pPhdrTmp;

	pContext->pPhdr[i].p_type  = type;
	pContext->pPhdr[i].p_flags = flags;
	pContext->pPhdr[i].p_align = 0x10;

	if(module_loader_is_elf(pContext) == 0){
		segment_info *pSegmentInfoTmp = (segment_info *)malloc(sizeof(segment_info) * (i + 1));
		memset(pSegmentInfoTmp, 0, sizeof(segment_info) * (i + 1));
		memcpy(pSegmentInfoTmp, pContext->pSegmentInfo, sizeof(segment_info) * i);
		free(pContext->pSegmentInfo);

		pContext->pSegmentInfo = pSegmentInfoTmp;
		pSegmentInfoTmp[i].offset = 0;
		pSegmentInfoTmp[i].length = 0;
		pSegmentInfoTmp[i].compression = 2; // 1 = uncompressed, 2 = compressed
		pSegmentInfoTmp[i].encryption  = 2; // 1 = encrypted,    2 = plain
	}

	pContext->pEhdr->e_phnum = i + 1;

	return i;
}

int module_loader_is_elf(const ModuleLoaderContext *pContext){
	return pContext->is_elf != 0;
}

int module_loader_open(const char *path, ModuleLoaderContext **ppResult){

	FILE *fd;
	void         *pHeader      = NULL;
	cf_header_t  *pCfHeader    = NULL;
	ext_header   *pExtHeader   = NULL;
	SCE_appinfo  *pAppInfo     = NULL;
	Elf32_Ehdr   *pEhdr        = NULL;
	Elf32_Phdr   *pPhdr        = NULL;
	segment_info *pSegmentInfo = NULL;
	SCE_version  *pVersion     = NULL;
	void         *pControlInfo = NULL;
	ModuleLoaderContext context;

	if(ppResult == NULL){
		printf_d("ppResult == NULL\n");
		return -1;
	}

	*ppResult = NULL;
	memset(&context, 0, sizeof(context));
	fd = NULL;

	cf_header_t cf_header;
	memset(&cf_header, 0, sizeof(cf_header));

	fd = fopen(path, "rb");
	if(fd == NULL){
		printf_e("self open failed\n");
		return -1;
	}

	printf_d("module file open success\n");

	if(fread(&cf_header, sizeof(cf_header), 1, fd) != 1){
		printf_e("File read error\n");
		goto error;
	}

	if(cf_header.base.m_magic != 0x454353 && cf_header.base.m_magic != 0x464C457F){
		printf_e("This self is not SCE self/elf\n");
		goto error;
	}

	int is_elf = cf_header.base.m_magic == 0x464C457F;
	if(is_elf == 0){
		if(cf_header.base.m_version != 3){
			printf_e("This self is not version 3\n");
			goto error;
		}

		if(cf_header.header_v3.m_certified_file_size > 0x10000000){
			printf_e("This self is too big\n");
			printf_e("Can't processing\n");
			goto error;
		}

		if(cf_header.header_v3.m_header_length > 0x1000){
			printf_e("This header is too big\n");
			printf_e("Can't processing\n");
			goto error;
		}

		pHeader = malloc(cf_header.header_v3.m_header_length);
		if(pHeader == NULL){
			printf_e("Cannot allocate memory for self header\n");
			goto error;
		}

		fseek(fd, 0LL, SEEK_SET);
		if(fread(pHeader, cf_header.header_v3.m_header_length, 1, fd) != 1){
			printf_e("self header : readbyte != cf_header.header_v3.m_header_length\n");
			goto error;
		}

		void *current_ptr = pHeader;

		pCfHeader = (cf_header_t *)current_ptr;
		current_ptr = (void *)(((uintptr_t)current_ptr) + sizeof(cf_header_t));

		pExtHeader = (ext_header *)current_ptr;
		current_ptr = (void *)(((uintptr_t)current_ptr) + sizeof(ext_header));

		pAppInfo = (SCE_appinfo *)malloc(sizeof(SCE_appinfo));
		if(pAppInfo == NULL){
			printf_e("Cannot allocate memory\n");
			goto error;
		}

		memcpy(pAppInfo, (SCE_appinfo *)((uintptr_t)pHeader + pExtHeader->appinfo_offset), sizeof(SCE_appinfo));

		pEhdr = (Elf32_Ehdr *)((uintptr_t)pHeader + pExtHeader->elf_offset);

		pPhdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * pEhdr->e_phnum);
		if(pPhdr == NULL){
			printf_e("Cannot allocate memory\n");
			goto error;
		}

		memcpy(pPhdr, (Elf32_Phdr *)((uintptr_t)pHeader + pExtHeader->phdr_offset), sizeof(Elf32_Phdr) * pEhdr->e_phnum);

		pSegmentInfo = (segment_info *)malloc(sizeof(segment_info) * pEhdr->e_phnum);
		if(pSegmentInfo == NULL){
			printf_e("Cannot allocate memory\n");
			goto error;
		}

		memcpy(pSegmentInfo, (segment_info *)((uintptr_t)pHeader + pExtHeader->section_info_offset), sizeof(Elf32_Phdr) * pEhdr->e_phnum);

		pVersion = (SCE_version *)malloc(sizeof(SCE_version));
		if(pVersion == NULL){
			printf_e("Cannot allocate memory\n");
			goto error;
		}

		memcpy(pVersion, (SCE_version *)((uintptr_t)pHeader + pExtHeader->sceversion_offset), sizeof(SCE_version));

		pControlInfo = malloc(pExtHeader->controlinfo_size);
		if(pControlInfo == NULL){
			printf_e("Cannot allocate memory\n");
			goto error;
		}

		memcpy(pControlInfo, (void *)((uintptr_t)pHeader + pExtHeader->controlinfo_offset), pExtHeader->controlinfo_size);

		printf_d("elf segments info\n");
		printf_d("segment num : 0x%X\n", pEhdr->e_phnum);

		for(int i=0;i<pEhdr->e_phnum;i++){
			printf_d("[%d] type  :0x%08X offset:0x%08X vaddr:0x%08X paddr:0x%08X\n",
				i,
				pPhdr[i].p_type,
				pPhdr[i].p_offset,
				pPhdr[i].p_vaddr,
				pPhdr[i].p_paddr
			);
			printf_d("    filesz:0x%08X memsz :0x%08X flags:0x%08X align:0x%08X\n",
				pPhdr[i].p_filesz,
				pPhdr[i].p_memsz,
				pPhdr[i].p_flags,
				pPhdr[i].p_align
			);
			printf_d("offset=0x%08lX length=0x%08lX compression=%ld encryption=%ld\n", pSegmentInfo[i].offset, pSegmentInfo[i].length, pSegmentInfo[i].compression, pSegmentInfo[i].encryption);

			context.segment[i].pData = malloc(pSegmentInfo[i].length);
			context.segment[i].memsz = pSegmentInfo[i].length;
			if(context.segment[i].pData == NULL){
				printf_e("Cannot allocate memory for segment\n");
				goto error;
			}

			if(pSegmentInfo[i].length != 0LL){
				fseek(fd, pSegmentInfo[i].offset, SEEK_SET);
				if(fread(context.segment[i].pData, pSegmentInfo[i].length, 1, fd) != 1){
					printf_e("segment %d read error (%d)\n", i, __LINE__);
					goto error;
				}
			}
		}
	}else{
		printf_i("elf mode\n");

		pHeader = malloc(0x34);
		if(pHeader == NULL){
			printf_e("Cannot allocate memory for self header\n");
			goto error;
		}

		fseek(fd, 0LL, SEEK_SET);
		if(fread(pHeader, 0x34, 1, fd) != 1){
			printf_e("elf header read error\n");
			goto error;
		}

		pEhdr = pHeader;

		if(pEhdr->e_type != 0xFE04){
			printf_e("elf is static?\n");
			goto error;
		}

		if(pEhdr->e_machine != EM_ARM){
			printf_e("Not ARM elf\n");
			goto error;
		}

		if(pEhdr->e_ehsize != 0x34){
			printf_e("Invalid elf header size = 0x%X\n", pEhdr->e_ehsize);
			goto error;
		}

		if(pEhdr->e_phentsize != sizeof(Elf32_Phdr)){
			printf_e("Invalid elf program header size = 0x%X\n", pEhdr->e_phentsize);
			goto error;
		}

		pPhdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * pEhdr->e_phnum);
		if(pPhdr == NULL){
			printf_e("cannot allocate memory for elf program header\n");
			goto error;
		}

		fseek(fd, pEhdr->e_phoff, SEEK_SET);
		if(fread(pPhdr, sizeof(Elf32_Phdr) * pEhdr->e_phnum, 1, fd) != 1){
			printf_e("Elf read failed\n");
			goto error;
		}

		for(int i=0;i<pEhdr->e_phnum;i++){
			printf_d("[%d] type  :0x%08X offset:0x%08X vaddr:0x%08X paddr:0x%08X\n",
				i,
				pPhdr[i].p_type,
				pPhdr[i].p_offset,
				pPhdr[i].p_vaddr,
				pPhdr[i].p_paddr
			);
			printf_d("    filesz:0x%08X memsz :0x%08X flags:0x%08X align:0x%08X\n",
				pPhdr[i].p_filesz,
				pPhdr[i].p_memsz,
				pPhdr[i].p_flags,
				pPhdr[i].p_align
			);

			context.segment[i].pData = malloc(pPhdr[i].p_filesz);
			context.segment[i].memsz = pPhdr[i].p_filesz;
			if(context.segment[i].pData == NULL){
				printf_e("Cannot allocate memory for segment\n");
				goto error;
			}

			if(pPhdr[i].p_filesz != 0){
				fseek(fd, pPhdr[i].p_offset, SEEK_SET);
				if(fread(context.segment[i].pData, pPhdr[i].p_filesz, 1, fd) != 1){
					printf_e("segment %d read error (%d)\n", i, __LINE__);
					goto error;
				}
			}
		}
	}

	printf_d("Segment read success\n");

	fclose(fd);
	fd = NULL;

	printf_d("file closed\n");
	printf_d("context value setting ...\n");

	context.is_elf       = is_elf;
	context.pHeader      = pHeader;
	context.pExtHeader   = pExtHeader;
	context.pEhdr        = pEhdr;
	context.pPhdr        = pPhdr;
	context.pSegmentInfo = pSegmentInfo;
	context.pAppInfo     = pAppInfo;
	context.pVersion     = pVersion;
	context.pControlInfo = pControlInfo;

	printf_d("context value setting ... ok\n");

	*ppResult = malloc(sizeof(context));
	if(*ppResult == NULL){
		printf_e("Cannot allocate memory\n");
		goto error;
	}

	printf_d("copy context to result ...\n");

	memcpy(*ppResult, &context, sizeof(context));

	printf_d("copy context to result ... ok\n");

	return 0;

error:
	if(fd != NULL){
		fclose(fd);
		fd = NULL;
	}

	for(int i=0;i<6;i++){
		if(context.segment[i].pData != NULL)
			free(context.segment[i].pData);
	}

	if(pControlInfo != NULL){
		free(pControlInfo);
		pControlInfo = NULL;
	}

	if(pVersion != NULL){
		free(pVersion);
		pVersion = NULL;
	}

	if(pSegmentInfo != NULL){
		free(pSegmentInfo);
		pSegmentInfo = NULL;
	}

	if(pPhdr != NULL){
		free(pPhdr);
		pPhdr = NULL;
	}

	if(pAppInfo != NULL){
		free(pAppInfo);
		pAppInfo = NULL;
	}

	if(pHeader != NULL){
		free(pHeader);
		pHeader = NULL;
	}

	return -1;
}

int module_loader_save(ModuleLoaderContext *pContext, const char *path){

	if(pContext == NULL || path == NULL)
		return -1;

	int elf_header_size = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum;

	// printf("elf_header_size=0x%X\n", elf_header_size);

	int segment_offset0 = elf_header_size;
	int segment_offset1 = elf_header_size + 0x1000;

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){

		segment_offset0 = (segment_offset0 + (pContext->pPhdr[i].p_align - 1)) & ~(pContext->pPhdr[i].p_align - 1);
		segment_offset1 = (segment_offset1 + (pContext->pPhdr[i].p_align - 1)) & ~(pContext->pPhdr[i].p_align - 1);

		// printf("0x%08X/0x%08X\n", segment_offset0, segment_offset1);

		pContext->pPhdr[i].p_offset      = segment_offset0;
		if(module_loader_is_elf(pContext) == 0)
			pContext->pSegmentInfo[i].offset = segment_offset1;

		segment_offset0 += pContext->pPhdr[i].p_filesz;
		if(module_loader_is_elf(pContext) == 0)
			segment_offset1 += pContext->pSegmentInfo[i].length;
	}

	FILE *fd;

	fd = fopen(path, "wb+");
	if(fd < 0){
		printf("cannot create output file\n");
		return -1;
	}

	uint64_t offset = sizeof(cf_header) + sizeof(ext_header);

	if(module_loader_is_elf(pContext) == 0){

		cf_header tmp_cf_header;
		memset(&tmp_cf_header, 0, sizeof(tmp_cf_header));
		memcpy(&tmp_cf_header, pContext->pHeader, sizeof(tmp_cf_header));

		tmp_cf_header.m_file_size           = segment_offset0;
		tmp_cf_header.m_certified_file_size = segment_offset1;

		fwrite(&tmp_cf_header, sizeof(tmp_cf_header), 1, fd);

		ext_header tmp_ext_header;
		memset(&tmp_ext_header, 0, sizeof(tmp_ext_header));

		tmp_ext_header.self_offset = 4;

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.appinfo_offset = offset;
		offset += sizeof(SCE_appinfo);

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.elf_offset = offset;
		offset += sizeof(Elf32_Ehdr);

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.phdr_offset = offset;
		offset += sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum;

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.shdr_offset = 0;
		offset += 0;

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.section_info_offset = offset;
		offset += sizeof(segment_info) * pContext->pEhdr->e_phnum;

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.sceversion_offset = offset;
		offset += sizeof(SCE_version);

		offset  = (offset + 0xF) & ~0xF;
		tmp_ext_header.controlinfo_offset = offset;
		tmp_ext_header.controlinfo_size = pContext->pExtHeader->controlinfo_size;
		offset += tmp_ext_header.controlinfo_size;

		fwrite(&tmp_ext_header, sizeof(tmp_ext_header), 1, fd);

		fseek(fd, tmp_ext_header.appinfo_offset, SEEK_SET);
		fwrite(pContext->pAppInfo, sizeof(SCE_appinfo), 1, fd);

		fseek(fd, tmp_ext_header.elf_offset, SEEK_SET);
		fwrite(pContext->pEhdr, sizeof(Elf32_Ehdr), 1, fd);

		fseek(fd, tmp_ext_header.phdr_offset, SEEK_SET);
		fwrite(pContext->pPhdr, ((sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum) + 0xF) & ~0xF, 1, fd);

		fseek(fd, tmp_ext_header.section_info_offset, SEEK_SET);
		fwrite(pContext->pSegmentInfo, sizeof(segment_info) * pContext->pEhdr->e_phnum, 1, fd);

		fseek(fd, tmp_ext_header.sceversion_offset, SEEK_SET);
		fwrite(pContext->pVersion, sizeof(SCE_version), 1, fd);

		fseek(fd, tmp_ext_header.controlinfo_offset, SEEK_SET);
		fwrite(pContext->pControlInfo, tmp_ext_header.controlinfo_size, 1, fd);

		fseek(fd, 0x1000, SEEK_SET);
		fwrite(pContext->pEhdr, sizeof(Elf32_Ehdr), 1, fd);
		fwrite(pContext->pPhdr, sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum, 1, fd);

		for(int i=0;i<pContext->pEhdr->e_phnum;i++){
			fseek(fd, pContext->pSegmentInfo[i].offset, SEEK_SET);
			fwrite(pContext->segment[i].pData, pContext->pSegmentInfo[i].length, 1, fd);
		}

	}else{
		fseek(fd, 0, SEEK_SET);
		fwrite(pContext->pEhdr, sizeof(Elf32_Ehdr), 1, fd);
		fwrite(pContext->pPhdr, sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum, 1, fd);

		for(int i=0;i<pContext->pEhdr->e_phnum;i++){
			fseek(fd, pContext->pPhdr[i].p_offset, SEEK_SET);
			fwrite(pContext->segment[i].pData, pContext->pPhdr[i].p_filesz, 1, fd);
		}
	}

	fclose(fd);
	fd = NULL;

	return 0;
}
