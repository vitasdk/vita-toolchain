#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "elf-utils.h"


int elf_utils_copy(Elf *dest, Elf *source)
{
	GElf_Ehdr ehdr;
	Elf_Scn *dst_scn, *src_scn;
	GElf_Shdr shdr;
	Elf_Data *dst_data, *src_data;
	size_t segment_count, segndx, new_segndx;
	GElf_Phdr phdr;

	ELF_ASSERT(elf_flagelf(dest, ELF_C_SET, ELF_F_LAYOUT));

	ELF_ASSERT(gelf_getehdr(source, &ehdr));
	ELF_ASSERT(gelf_newehdr(dest, gelf_getclass(source)));
	ELF_ASSERT(gelf_update_ehdr(dest, &ehdr));

	src_scn = NULL;
	while ((src_scn = elf_nextscn(source, src_scn)) != NULL) {
		ELF_ASSERT(gelf_getshdr(src_scn, &shdr));
		ELF_ASSERT(dst_scn = elf_newscn(dest));
		ELF_ASSERT(gelf_update_shdr(dst_scn, &shdr));

		src_data = NULL;
		while ((src_data = elf_getdata(src_scn, src_data)) != NULL) {
			ELF_ASSERT(dst_data = elf_newdata(dst_scn));
			memcpy(dst_data, src_data, sizeof(Elf_Data));
		}
	}

	ELF_ASSERT(elf_getphdrnum(source, &segment_count) == 0);

	// only count PT_LOAD segments
	new_segndx = 0;
	for (segndx = 0; segndx < segment_count; segndx++) {
		ELF_ASSERT(gelf_getphdr(source, segndx, &phdr));
		if (phdr.p_type == PT_LOAD) {
			new_segndx++;
		}
	}
	ASSERT(new_segndx > 0);

	// copy PT_LOAD segments
	ELF_ASSERT(gelf_newphdr(dest, new_segndx));
	new_segndx = 0;
	for (segndx = 0; segndx < segment_count; segndx++) {
		ELF_ASSERT(gelf_getphdr(source, segndx, &phdr));
		if (phdr.p_type == PT_LOAD) {
			ELF_ASSERT(gelf_update_phdr(dest, new_segndx, &phdr));
			new_segndx++;
		}
	}
		
	return 1;
failure:
	return 0;
}

Elf *elf_utils_copy_to_file(const char *filename, Elf *source, FILE **file)
{
	Elf *dest = NULL;

	*file = fopen(filename, "wb");
	if (*file == NULL){
		fprintf(stderr, "Could not open %s for writing\n",filename);
		goto failure;
	}


	ELF_ASSERT(elf_version(EV_CURRENT) != EV_NONE);
	ELF_ASSERT(dest = elf_begin(fileno(*file), ELF_C_WRITE, NULL));

	if (!elf_utils_copy(dest, source))
		goto failure;

	return dest;
failure:
	if (dest != NULL)
		elf_end(dest);
	return NULL;
}

int elf_utils_duplicate_scn_contents(Elf *e, int scndx)
{
	Elf_Scn *scn;
	Elf_Data *data;
	void *new_data;

	ELF_ASSERT(scn = elf_getscn(e, scndx));

	data = NULL;
	while ((data = elf_getdata(scn, data)) != NULL) {
		ASSERT(new_data = malloc(data->d_size));
		memcpy(new_data, data->d_buf, data->d_size);
		data->d_buf = new_data;
	}
		
	return 1;
failure:
	return 0;
}

void elf_utils_free_scn_contents(Elf *e, int scndx)
{
	Elf_Scn *scn;
	Elf_Data *data;

	ELF_ASSERT(scn = elf_getscn(e, scndx));

	data = NULL;
	while ((data = elf_getdata(scn, data)) != NULL) {
		free(data->d_buf);
		data->d_buf = NULL;
	}
		
failure:
	return;
}

int elf_utils_duplicate_shstrtab(Elf *e)
{
	size_t shstrndx;

	ELF_ASSERT(elf_getshdrstrndx(e, &shstrndx) == 0);

	return elf_utils_duplicate_scn_contents(e, shstrndx);
failure:
	return 0;
}

int elf_utils_shift_contents(Elf *e, int start_offset, int shift_amount)
{
	GElf_Ehdr ehdr;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	size_t segment_count = 0, segndx;
	GElf_Phdr phdr;
	int bottom_section_offset = 0;
	GElf_Xword sh_size;

	ELF_ASSERT(gelf_getehdr(e, &ehdr));
	if (ehdr.e_shoff >= start_offset) {
		ehdr.e_shoff += shift_amount;
		ELF_ASSERT(gelf_update_ehdr(e, &ehdr));
	}

	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		ELF_ASSERT(gelf_getshdr(scn, &shdr));
		if (shdr.sh_offset >= start_offset) {
			shdr.sh_offset += shift_amount;
			ELF_ASSERT(gelf_update_shdr(scn, &shdr));
		}
		sh_size = (shdr.sh_type == SHT_NOBITS) ? 0 : shdr.sh_size;
		if (shdr.sh_offset + sh_size > bottom_section_offset) {
			bottom_section_offset = shdr.sh_offset + sh_size;
		}
	}

	if (bottom_section_offset > ehdr.e_shoff) {
		ELF_ASSERT(gelf_getehdr(e, &ehdr));
		ehdr.e_shoff = bottom_section_offset;
		ELF_ASSERT(gelf_update_ehdr(e, &ehdr));
	}

	/* A bug in libelf means that getphdrnum will report failure in a new file.
	 * However, it will still set segment_count, so we'll use it. */
	ELF_ASSERT((elf_getphdrnum(e, &segment_count), segment_count > 0));

	for (segndx = 0; segndx < segment_count; segndx++) {
		ELF_ASSERT(gelf_getphdr(e, segndx, &phdr));
		if (phdr.p_offset >= start_offset) {
			phdr.p_offset += shift_amount;
			ELF_ASSERT(gelf_update_phdr(e, segndx, &phdr));
		}
	}
		
	return 1;
failure:
	return 0;
}

Elf_Scn *elf_utils_new_scn_with_name(Elf *e, const char *scn_name)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;
	size_t shstrndx, index, namelen;
	Elf_Data *shstrdata;
	void *ptr;

	ELF_ASSERT(elf_getshdrstrndx(e, &shstrndx) == 0);

	ELF_ASSERT(scn = elf_getscn(e, shstrndx));
	ELF_ASSERT(shstrdata = elf_getdata(scn, NULL));

	namelen = strlen(scn_name) + 1;
	ELF_ASSERT(gelf_getshdr(scn, &shdr));
	if (!elf_utils_shift_contents(e, shdr.sh_offset + shdr.sh_size, namelen))
		goto failure;
	ASSERT(ptr = realloc(shstrdata->d_buf, shstrdata->d_size + namelen));
	index = shstrdata->d_size;
	strcpy(ptr+index, scn_name);
	shstrdata->d_buf = ptr;
	shstrdata->d_size += namelen;
	shdr.sh_size += namelen;
	ELF_ASSERT(gelf_update_shdr(scn, &shdr));

	ELF_ASSERT(scn = elf_newscn(e));
	ELF_ASSERT(gelf_getshdr(scn, &shdr));
	shdr.sh_name = index;
	ELF_ASSERT(gelf_update_shdr(scn, &shdr));
	
	return scn;
failure:
	return NULL;
}

Elf_Scn *elf_utils_new_scn_with_data(Elf *e, const char *scn_name, void *buf, int len)
{
	Elf_Scn *scn;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf_Data *data;
	int offset;

	scn = elf_utils_new_scn_with_name(e, scn_name);
	if (scn == NULL)
		goto failure;

	ELF_ASSERT(gelf_getehdr(e, &ehdr));
	offset = ehdr.e_shoff;
	if (!elf_utils_shift_contents(e, offset, len + 0x10))
		goto failure;

	ELF_ASSERT(gelf_getshdr(scn, &shdr));
	shdr.sh_offset = (offset + 0x10) & ~0xF;
	shdr.sh_size = len;
	shdr.sh_addralign = 1;
	ELF_ASSERT(gelf_update_shdr(scn, &shdr));

	ELF_ASSERT(data = elf_newdata(scn));
	data->d_buf = buf;
	data->d_type = ELF_T_BYTE;
	data->d_version = EV_CURRENT;
	data->d_size = len;
	data->d_off = 0;
	data->d_align = 1;

	return scn;
failure:
	return NULL;
}
