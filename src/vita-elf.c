#include <stdlib.h>
#include <string.h>

#include <libelf.h>
#include <gelf.h>
/* Note:
 * Even though we know the Vita is a 32-bit platform and we specifically check
 * that we're operating on a 32-bit ELF only, we still use the GElf family of
 * functions.  This is because they have extra sanity checking baked in.
 */

#ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_NORESERVE
#  define MAP_NORESERVE 0
#endif

#ifndef __MINGW32__
/* This may cause trouble with Windows portability, but there are Windows alternatives
 * to mmap() that we can explore later.  It'll probably work under Cygwin.
 */
#include <sys/mman.h>
#else
#define mmap(ptr,size,c,d,e,f) malloc(size)
#define munmap(ptr, size) free(ptr)
#endif

#include "vita-elf.h"
#include "vita-import.h"
#include "vita-export.h"
#include "elf-defs.h"
#include "fail-utils.h"
#include "endian-utils.h"
#include "sha256.h"
#include "vita-export.h"
#include "sce-elf.h"

static void free_rela_table(vita_elf_rela_table_t *rtable);

static int fixup_vstub_rela(vita_elf_t *ve, uint8_t *code, Elf32_Addr rel_vaddr)
{
	Elf_Scn *scn = NULL;
	Elf_Data *data = NULL;
	GElf_Shdr shdr;
	union {
		Elf32_Word arm;
		Elf32_Half thumb[2];
	} instr;
	void *rel_addr = NULL;
	Elf32_Word rel_offset;

	while ((scn = elf_nextscn(ve->elf, scn)) != NULL) {
		ELF_ASSERT(gelf_getshdr(scn, &shdr) == &shdr);
		if ((shdr.sh_addr <= rel_vaddr) && (rel_vaddr < shdr.sh_addr + shdr.sh_size)) {
			rel_offset = rel_vaddr - shdr.sh_addr;
			while ((data = elf_getdata(scn, data)) != NULL) {
				if ((data->d_off <= rel_offset) && (rel_offset < data->d_off + data->d_size)) {
					rel_addr = data->d_buf + (rel_offset - data->d_off);
					break;
				}
			}
			break;
		}
	}

	if (rel_addr == NULL)
		FAILX("Failed to find address of relocatable");

	/* Data relocations will be stubbed NULL access */
	switch (*code) {
	case R_ARM_TARGET1: /* Target 1 is handled like ABS32 on Vita*/
	case R_ARM_ABS32:
		*(Elf32_Word *)rel_addr = 0;

		*code = R_ARM_NONE;
		break;
	case R_ARM_TARGET2: /* Target 2 is handled like REL32 on Vita*/
	case R_ARM_REL32:
		*(Elf32_Word *)rel_addr = 0 - rel_vaddr;

		*code = R_ARM_NONE;
		break;
	case R_ARM_MOVW_ABS_NC:
	case R_ARM_MOVT_ABS:
		instr.arm = le32toh(*(Elf32_Word *)rel_addr);
		instr.arm &= 0xFFF0F000; /* Mask out the immediate */

		*(Elf32_Word *)rel_addr = htole32(instr.arm);

		*code = R_ARM_NONE;
		break;
	case R_ARM_THM_MOVW_ABS_NC:
	case R_ARM_THM_MOVT_ABS:
		instr.thumb[0] = le16toh(*(Elf32_Half *)rel_addr);
		instr.thumb[1] = le16toh(*(Elf32_Half *)(rel_addr + 2));
		instr.thumb[0] &= 0xFBF0;
		instr.thumb[1] &= 0x8F00; /* Mask out the immediate */

		*(Elf32_Half *)rel_addr = htole16(instr.thumb[0]);
		*(Elf32_Half *)(rel_addr + 2) = htole16(instr.thumb[1]);

		*code = R_ARM_NONE;
		break;
	default: /* Other relocations will be left as is */
		break;
	}

	return 1;
failure:
	return 0;
}

static void create_vstub_short_rel(vita_elf_t *ve, vita_elf_stub_t *vstub, vita_elf_rela_t *rela)
{
	SCE_Rel *rel;

	vstub->rel_info_size += sizeof(Elf32_Word) * 2;
	vstub->rel_info = realloc(vstub->rel_info, (vstub->rel_count + 1) * sizeof(SCE_Rel));

	rel = (SCE_Rel *)vstub->rel_info + vstub->rel_count++;
	rel->r_short = 1;
	rel->r_variable_short_entry.r_code = rela->type;
	rel->r_variable_short_entry.r_datseg = vita_elf_vaddr_to_segndx(ve, rela->offset);
	rel->r_variable_short_entry.r_offset = vita_elf_vaddr_to_segoffset(ve, rela->offset, rel->r_variable_short_entry.r_datseg);
	rel->r_variable_short_entry.r_addend = *(Elf32_Word *)(&rela->addend);
}

static void create_vstub_long_rel(vita_elf_t *ve, vita_elf_stub_t *vstub, vita_elf_rela_t *rela)
{
	SCE_Rel *rel;

	vstub->rel_info_size += sizeof(Elf32_Word) * 3;
	vstub->rel_info = realloc(vstub->rel_info, (vstub->rel_count + 1) * sizeof(SCE_Rel));

	rel = ((SCE_Rel *)vstub->rel_info) + vstub->rel_count++;
	rel->r_short = 2;
	rel->r_variable_long_entry.r_code = rela->type;
	rel->r_variable_long_entry.r_datseg = vita_elf_vaddr_to_segndx(ve, rela->offset);
	rel->r_variable_long_entry.r_offset = vita_elf_vaddr_to_segoffset(ve, rela->offset, rel->r_variable_short_entry.r_datseg);
	rel->r_variable_long_entry.r_addend = *(Elf32_Word *)(&rela->addend);
}

static int lookup_vstub_relas(vita_elf_t *ve)
{
	vita_elf_stub_t *vstub;
	vita_elf_rela_table_t *rtable;
	vita_elf_rela_t *rela;

	for (int i = 0; i < ve->num_vstubs; i++) {
		vstub = &(ve->vstubs[i]);
		rtable = ve->rela_tables;
		while (rtable != NULL) {
			for (int j = 0; j < rtable->num_relas; j++) {
				rela = &(rtable->relas[j]);
				if (rela->symbol == vstub->symbol) {
					if ((rela->addend >= -32768) && (rela->addend < 32768)) /* Addend is fully representable with 16 signed bits */
						create_vstub_short_rel(ve, vstub, rela);
					else
						create_vstub_long_rel(ve, vstub, rela);

					if (fixup_vstub_rela(ve, &rela->type, rela->offset) == 0)
						FAILX("Failed to fixup vstub relocations");
				}
			}
			rtable = rtable->next;
		}
	}

	return 1;

failure:
	return 0;
}

static int load_stubs(Elf_Scn *scn, int *num_stubs, vita_elf_stub_t **stubs, char *name)
{
	GElf_Shdr shdr;
	Elf_Data *data;
	uint32_t *stub_data;
	int chunk_offset, total_bytes;
	vita_elf_stub_t *curstub;
	int old_num;
	size_t shndx;

	gelf_getshdr(scn, &shdr);
	shndx = elf_ndxscn(scn);

	old_num = *num_stubs;
	*num_stubs = old_num + shdr.sh_size / 16;
	*stubs = realloc(*stubs, *num_stubs * sizeof(vita_elf_stub_t));
	memset(&(*stubs)[old_num], 0, sizeof(vita_elf_stub_t) * shdr.sh_size / 16);
	
	name = strrchr(name,'.')+1;
	
	curstub = *stubs;
	curstub = &curstub[*num_stubs - (shdr.sh_size / 16)];
	
	data = NULL; total_bytes = 0;
	while (total_bytes < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {

		for (stub_data = (uint32_t *)data->d_buf, chunk_offset = 0;
				chunk_offset < data->d_size;
				stub_data += 4, chunk_offset += 16) {
			curstub->addr = shdr.sh_addr + data->d_off + chunk_offset;
			curstub->library = vita_imports_lib_new(name,false,0,0,0);
			curstub->library->flags = le32toh(stub_data[0]);
			curstub->library_nid = le32toh(stub_data[1]);
			curstub->target_nid = le32toh(stub_data[2]);
			curstub->shndx = shndx;
			curstub++;
		}

		total_bytes += data->d_size;
	}

	return 1;
}

static int load_symbols(vita_elf_t *ve, Elf_Scn *scn)
{
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	int total_bytes;
	int data_beginsym, symndx;
	vita_elf_symbol_t *cursym;

	if (elf_ndxscn(scn) == ve->symtab_ndx)
		return 1; /* Already loaded */

	if (ve->symtab != NULL)
		FAILX("ELF file appears to have multiple symbol tables!");

	gelf_getshdr(scn, &shdr);

	ve->num_symbols = shdr.sh_size / shdr.sh_entsize;
	ve->symtab = calloc(ve->num_symbols, sizeof(vita_elf_symbol_t));
	ve->symtab_ndx = elf_ndxscn(scn);

	data = NULL; total_bytes = 0;
	while (total_bytes < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {

		data_beginsym = data->d_off / shdr.sh_entsize;
		for (symndx = 0; symndx < data->d_size / shdr.sh_entsize; symndx++) {
			if (gelf_getsym(data, symndx, &sym) != &sym)
				FAILE("gelf_getsym() failed");

			cursym = ve->symtab + symndx + data_beginsym;

			cursym->name = elf_strptr(ve->elf, shdr.sh_link, sym.st_name);
			cursym->value = sym.st_value;
			cursym->type = GELF_ST_TYPE(sym.st_info);
			cursym->binding = GELF_ST_BIND(sym.st_info);
			cursym->shndx = sym.st_shndx;
			cursym->visibility = ELF32_ST_VISIBILITY(sym.st_other);
		}

		total_bytes += data->d_size;
	}

	return 1;
failure:
	return 0;
}

#define THUMB_SHUFFLE(x) ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))
static uint32_t decode_rel_target(uint32_t data, int type, uint32_t addr)
{
	uint32_t upper, lower, sign, j1, j2, imm10, imm11;
	switch(type) {
		case R_ARM_NONE:
		case R_ARM_V4BX:
			return 0xdeadbeef;
		case R_ARM_ABS32:
		case R_ARM_TARGET1:
			return data;
		case R_ARM_REL32:
		case R_ARM_TARGET2:
			return data + addr;
		case R_ARM_PREL31:
			return data + addr;
		case R_ARM_THM_CALL: // bl (THUMB)
			data = THUMB_SHUFFLE(data);
			upper = data >> 16;
			lower = data & 0xFFFF;
			sign = (upper >> 10) & 1;
			j1 = (lower >> 13) & 1;
			j2 = (lower >> 11) & 1;
			imm10 = upper & 0x3ff;
			imm11 = lower & 0x7ff;
			return addr + (((imm11 | (imm10 << 11) | (!(j2 ^ sign) << 21) | (!(j1 ^ sign) << 22) | (sign << 23)) << 1) | (sign ? 0xff000000 : 0));
		case R_ARM_CALL: // bl/blx
		case R_ARM_JUMP24: // b/bl<cond>
			data = (data & 0x00ffffff) << 2;
			// if we got a negative value, sign extend it
			if (data & (1 << 25))
				data |= 0xfc000000;
			return data + addr;
		case R_ARM_MOVW_ABS_NC: //movw
			return ((data & 0xf0000) >> 4) | (data & 0xfff);
		case R_ARM_MOVT_ABS: //movt
			return (((data & 0xf0000) >> 4) | (data & 0xfff)) << 16;
		case R_ARM_THM_MOVW_ABS_NC: //MOVW (THUMB)
			data = THUMB_SHUFFLE(data);
			return (((data >> 16) & 0xf) << 12)
				| (((data >> 26) & 0x1) << 11)
				| (((data >> 12) & 0x7) << 8)
				| (data & 0xff);
		case R_ARM_THM_MOVT_ABS: //MOVT (THUMB)
			data = THUMB_SHUFFLE(data);
			return (((data >> 16) & 0xf) << 28)
				| (((data >> 26) & 0x1) << 27)
				| (((data >> 12) & 0x7) << 24)
				| ((data & 0xff) << 16);
	}

	errx(EXIT_FAILURE, "Invalid relocation type: %d", type);
}

#define REL_HANDLE_NORMAL 0
#define REL_HANDLE_IGNORE -1
#define REL_HANDLE_INVALID -2
static int get_rel_handling(int type)
{
	switch(type) {
		case R_ARM_NONE:
		case R_ARM_V4BX:
			return REL_HANDLE_IGNORE;
		case R_ARM_ABS32:
		case R_ARM_TARGET1:
		case R_ARM_REL32:
		case R_ARM_TARGET2:
		case R_ARM_PREL31:
		case R_ARM_THM_CALL:
		case R_ARM_CALL:
		case R_ARM_JUMP24:
		case R_ARM_MOVW_ABS_NC:
		case R_ARM_MOVT_ABS:
		case R_ARM_THM_MOVW_ABS_NC:
		case R_ARM_THM_MOVT_ABS:
			return REL_HANDLE_NORMAL;
	}

	return REL_HANDLE_INVALID;
}

static int load_rel_table(vita_elf_t *ve, Elf_Scn *scn)
{
	Elf_Scn *text_scn;
	GElf_Shdr shdr, text_shdr;
	Elf_Data *data, *text_data;
	GElf_Rel rel;
	int relndx;

	int rel_sym;
	int handling;

	vita_elf_rela_table_t *rtable = NULL;
	vita_elf_rela_t *currela = NULL;
	uint32_t insn, target = 0;

	gelf_getshdr(scn, &shdr);

	if (!load_symbols(ve, elf_getscn(ve->elf, shdr.sh_link)))
		goto failure;

	rtable = calloc(1, sizeof(vita_elf_rela_table_t));
	ASSERT(rtable != NULL);
	rtable->num_relas = shdr.sh_size / shdr.sh_entsize;
	rtable->relas = calloc(rtable->num_relas, sizeof(vita_elf_rela_t));
	ASSERT(rtable->relas != NULL);

	rtable->target_ndx = shdr.sh_info;
	text_scn = elf_getscn(ve->elf, shdr.sh_info);
	gelf_getshdr(text_scn, &text_shdr);
	text_data = elf_getdata(text_scn, NULL);

	/* We're blatantly assuming here that both of these sections will store
	 * the entirety of their data in one Elf_Data item.  This seems to be true
	 * so far in my testing, and from the libelf source it looks like it's
	 * unlikely to allocate multiple data items on initial file read, but
	 * should be fixed someday. */
	data = elf_getdata(scn, NULL);
	for (relndx = 0; relndx < data->d_size / shdr.sh_entsize; relndx++) {
		if (gelf_getrel(data, relndx, &rel) != &rel)
			FAILX("gelf_getrel() failed");

		if ((rel.r_offset - text_shdr.sh_addr) >= text_shdr.sh_size)
			continue;

		currela = rtable->relas + relndx;
		currela->type = GELF_R_TYPE(rel.r_info);
		/* R_ARM_THM_JUMP24 is functionally the same as R_ARM_THM_CALL, however Vita only supports the second one */
		if (currela->type == R_ARM_THM_JUMP24)
			currela->type = R_ARM_THM_CALL;
		/* This one comes from libstdc++.
		 * Should be safe to ignore because it's pc-relative and already encoded in the file. */
		if (currela->type == R_ARM_THM_PC11)
			continue;
		currela->offset = rel.r_offset;

		/* Use memcpy for unaligned relocation. */
		memcpy(&insn, text_data->d_buf+(rel.r_offset - text_shdr.sh_addr), sizeof(insn));
		insn = le32toh(insn);

		handling = get_rel_handling(currela->type);

		if (handling == REL_HANDLE_IGNORE)
			continue;
		else if (handling == REL_HANDLE_INVALID)
			FAILX("Invalid relocation type %d!", currela->type);

		rel_sym = GELF_R_SYM(rel.r_info);
		if (rel_sym >= ve->num_symbols)
			FAILX("REL entry tried to access symbol %d, but only %d symbols loaded", rel_sym, ve->num_symbols);

		currela->symbol = ve->symtab + rel_sym;

		target = decode_rel_target(insn, currela->type, rel.r_offset);

		/* From some testing the added for MOVT/MOVW should actually always be 0 */
		if (currela->type == R_ARM_MOVT_ABS || currela->type == R_ARM_THM_MOVT_ABS)
			currela->addend = target - (currela->symbol->value & 0xFFFF0000);
		else if (currela->type == R_ARM_MOVW_ABS_NC || currela->type == R_ARM_THM_MOVW_ABS_NC)
			currela->addend = target - (currela->symbol->value & 0xFFFF);
		/* Symbol value could be OR'ed with 1 if the function is compiled in Thumb mode,
		 * however for the relocation addend we need the actual address. */
		else if (currela->type == R_ARM_THM_CALL)
			currela->addend = target - (currela->symbol->value & 0xFFFFFFFE);
		else
			currela->addend = target - currela->symbol->value;
	}

	rtable->next = ve->rela_tables;
	ve->rela_tables = rtable;

	return 1;
failure:
	free_rela_table(rtable);
	return 0;
}

static int load_rela_table(vita_elf_t *ve, Elf_Scn *scn)
{
	warnx("RELA sections currently unsupported");
	return 0;
}

static int lookup_stub_symbols(vita_elf_t *ve, int num_stubs, vita_elf_stub_t *stubs, varray *stubs_va, int sym_type)
{
	int symndx;
	vita_elf_symbol_t *cursym;
	int stub, stubs_ndx, i, *cur_ndx;

	for (symndx = 0; symndx < ve->num_symbols; symndx++) {
		cursym = ve->symtab + symndx;

		if (cursym->binding != STB_GLOBAL)
			continue;
		if (cursym->type != STT_FUNC && cursym->type != STT_OBJECT)
			continue;
		stubs_ndx = -1;
		
		for(i=0;i<stubs_va->count;i++){
			cur_ndx = VARRAY_ELEMENT(stubs_va,i);
			if (cursym->shndx == *cur_ndx){
				stubs_ndx = cursym->shndx;
				break;
			}
		}
		
		if(stubs_ndx == -1)
			continue;	
			
		if (cursym->type != sym_type)
			FAILX("Global symbol %s in section %d expected to have type %s; instead has type %s",
					cursym->name, stubs_ndx, elf_decode_st_type(sym_type), elf_decode_st_type(cursym->type));
		
		for (stub = 0; stub < num_stubs; stub++) {
			if (stubs[stub].addr != cursym->value || stubs[stub].shndx != cursym->shndx)
				continue;
			if (stubs[stub].symbol != NULL)
				FAILX("Stub at %06x in section %d has duplicate symbols: %s, %s",
						cursym->value, stubs_ndx, stubs[stub].symbol->name, cursym->name);
			stubs[stub].symbol = cursym;
			cursym->stub = &stubs[stub];
			break;
		}

		if (stub == num_stubs)
			FAILX("Global symbol %s in section %d not pointing to a valid stub",
					cursym->name, cursym->shndx);
	}

	return 1;

failure:
	return 0;
}

static int is_valid_relsection(vita_elf_t *ve, GElf_Shdr *rel) {
	Elf_Scn *info;
	size_t phnum;
	GElf_Shdr shdr;
	GElf_Phdr phdr;

	ASSERT(rel->sh_type == SHT_REL || rel->sh_type == SHT_RELA);
	ELF_ASSERT(info = elf_getscn(ve->elf, rel->sh_info));
	ELF_ASSERT(gelf_getshdr(info, &shdr));
	ELF_ASSERT(elf_getphdrnum(ve->elf, &phnum) == 0);

	if (shdr.sh_type == SHT_NOBITS) {
		shdr.sh_size = 0;
	}

	// Here we assume that every section falls into EXACTLY one segment
	// However, the ELF format allows for weird things like one section that 
	// is partially in one segment and partially in another one (or even 
	// multiple).
	// TODO: For robustness, consider these corner cases
	// Right now, we just assume if you have one of these messed up ELFs, 
	// we don't support it because it's likely to break in other parts of this 
	// code :)
	for (int i = 0; i < phnum; i++) {
		ELF_ASSERT(gelf_getphdr(ve->elf, i, &phdr));
		if (phdr.p_type != PT_LOAD) {
			continue;
		}
		// TODO: Take account of possible integer wrap-arounds
		if (phdr.p_offset <= shdr.sh_offset && shdr.sh_offset + shdr.sh_size <= phdr.p_offset + phdr.p_filesz) {
			return 1;
		}
	}

failure:
	return 0;
}

vita_elf_t *vita_elf_load(const char *filename, int check_stub_count)
{
	vita_elf_t *ve = NULL;
	GElf_Ehdr ehdr;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	size_t shstrndx;
	char *name;
	const char **debug_name;

	GElf_Phdr phdr;
	size_t segment_count, segndx, loaded_segments;
	vita_elf_segment_info_t *curseg;


	if (elf_version(EV_CURRENT) == EV_NONE)
		FAILX("ELF library initialization failed: %s", elf_errmsg(-1));

	ve = calloc(1, sizeof(vita_elf_t));
	ASSERT(ve != NULL);
	
	ASSERT(varray_init(&ve->fstubs_va, sizeof(int), 8));
	ASSERT(varray_init(&ve->vstubs_va, sizeof(int), 4));

	if ((ve->file = fopen(filename, "rb")) == NULL)
		FAIL("open %s failed", filename);

	ELF_ASSERT(ve->elf = elf_begin(fileno(ve->file), ELF_C_READ, NULL));

	if (elf_kind(ve->elf) != ELF_K_ELF)
		FAILX("%s is not an ELF file", filename);

	ELF_ASSERT(gelf_getehdr(ve->elf, &ehdr));

	if (ehdr.e_machine != EM_ARM)
		FAILX("%s is not an ARM binary", filename);

	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 || ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		FAILX("%s is not a 32-bit, little-endian binary", filename);

	ELF_ASSERT(elf_getshdrstrndx(ve->elf, &shstrndx) == 0);

	scn = NULL;

	while ((scn = elf_nextscn(ve->elf, scn)) != NULL) {
		ELF_ASSERT(gelf_getshdr(scn, &shdr));

		ELF_ASSERT(name = elf_strptr(ve->elf, shstrndx, shdr.sh_name));

		if (shdr.sh_type == SHT_PROGBITS && strncmp(name, ".vitalink.fstubs", strlen(".vitalink.fstubs")) == 0) {
			int ndxscn = elf_ndxscn(scn);
			varray_push(&ve->fstubs_va,&ndxscn);
			if (!load_stubs(scn, &ve->num_fstubs, &ve->fstubs, name))
				goto failure;
		} else if (shdr.sh_type == SHT_PROGBITS && strncmp(name, ".vitalink.vstubs", strlen(".vitalink.vstubs")) == 0) {
			int ndxscn = elf_ndxscn(scn);
			varray_push(&ve->vstubs_va,&ndxscn);
			if (!load_stubs(scn, &ve->num_vstubs, &ve->vstubs, name))
				goto failure;
		}

		if (shdr.sh_type == SHT_SYMTAB) {
			if (!load_symbols(ve, scn))
				goto failure;
		} else if (shdr.sh_type == SHT_REL) {
			if (!is_valid_relsection(ve, &shdr))
				continue;
			if (!load_rel_table(ve, scn))
				goto failure;
		} else if (shdr.sh_type == SHT_RELA) {
			if (!is_valid_relsection(ve, &shdr))
				continue;
			if (!load_rela_table(ve, scn))
				goto failure;
		}
	}

	if (ve->fstubs_va.count == 0 && ve->vstubs_va.count == 0 && check_stub_count)
		FAILX("No .vitalink stub sections in binary, probably not a Vita binary. If this is a vita binary, pass '-n' to squash this error.");

	if (ve->symtab == NULL)
		FAILX("No symbol table in binary, perhaps stripped out");

	if (ve->rela_tables == NULL)
		FAILX("No relocation sections in binary; use -Wl,-q while compiling");

	if (ve->fstubs_va.count != 0) {
		if (!lookup_stub_symbols(ve, ve->num_fstubs, ve->fstubs, &ve->fstubs_va, STT_FUNC)) goto failure;
	}

	if (ve->vstubs_va.count != 0) {
		if (!lookup_stub_symbols(ve, ve->num_vstubs, ve->vstubs, &ve->vstubs_va, STT_OBJECT)) goto failure;
	}

	ELF_ASSERT(elf_getphdrnum(ve->elf, &segment_count) == 0);

	ve->segments = calloc(segment_count, sizeof(vita_elf_segment_info_t));
	ASSERT(ve->segments != NULL);
	loaded_segments = 0;

	for (segndx = 0; segndx < segment_count; segndx++) {
		ELF_ASSERT(gelf_getphdr(ve->elf, segndx, &phdr));

		if (phdr.p_type != PT_LOAD) {
			continue; // skip non-loadable segments
		}

		curseg = ve->segments + loaded_segments;
		curseg->type = phdr.p_type;
		curseg->vaddr = phdr.p_vaddr;
		curseg->memsz = phdr.p_memsz;

		if (curseg->memsz) {
			curseg->vaddr_top = mmap(NULL, curseg->memsz, /* PROT_NONE */ PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
			if (curseg->vaddr_top == NULL)
				FAIL("Could not allocate address space for segment %d", (int)segndx);
			curseg->vaddr_bottom = curseg->vaddr_top + curseg->memsz;

			memset(curseg->vaddr_top, 0, curseg->memsz);
			if (phdr.p_filesz != 0) {
				fseek(ve->file, phdr.p_offset, SEEK_SET);
				if (fread((void *)(curseg->vaddr_top), phdr.p_filesz, 1, ve->file) != 1) {
					FAIL("Could not read segment %d", (int)segndx);
				}
			}
		}
		
		loaded_segments++;
	}
	ve->num_segments = loaded_segments;

	/* This part can only be done after the segments have been loaded */
	if (lookup_vstub_relas(ve) == 0)
		FAILX("Failed to lookup the vstub relocations");

	return ve;

failure:
	if (ve != NULL)
		vita_elf_free(ve);
	return NULL;
}

static void free_rela_table(vita_elf_rela_table_t *rtable)
{
	if (rtable == NULL) return;
	free(rtable->relas);
	free_rela_table(rtable->next);
	free(rtable);
}

void vita_elf_free(vita_elf_t *ve)
{
	int i;

	for (i = 0; i < ve->num_segments; i++) {
		if (ve->segments[i].vaddr_top != NULL)
			munmap((void *)ve->segments[i].vaddr_top, ve->segments[i].memsz);
	}

	for (i = 0; i < ve->num_vstubs; i++)
		free(ve->vstubs[i].rel_info);

	/* free() is safe to call on NULL */
	free(ve->fstubs);
	free(ve->vstubs);
	free(ve->symtab);
	if (ve->elf != NULL)
		elf_end(ve->elf);
	if (ve->file != NULL)
		fclose(ve->file);
	free(ve);
}

static int compar_export_symbols(const void * a, const void *b)
{
	vita_export_symbol *symA = (*(vita_export_symbol **)a);
	vita_export_symbol *symB = (*(vita_export_symbol **)b);

	if (symA->nid > symB->nid)
		return 1;
	if (symA->nid < symB->nid)
		return -1;

	return 0;
}

void vita_elf_generate_exports(vita_elf_t *ve, vita_export_t *exports)
{
	int i;
	vita_elf_symbol_t *cursym;
	vita_library_export *exportlib = NULL;
	vita_export_symbol *exportSym;
	size_t strLen;

	for (i = 0; i < ve->num_symbols; i++) {
		cursym = &ve->symtab[i];
		if (cursym->stub != NULL)
			continue;
		if (cursym->type != STT_FUNC && cursym->type != STT_OBJECT)
			continue;
		if (cursym->binding != STB_GLOBAL)
			continue;

		if (strcmp(cursym->name, "module_start") == 0 && exports->start == NULL) {
			exports->start = strdup(cursym->name);
			continue;
		}
		if (strcmp(cursym->name, "module_stop") == 0 && exports->stop == NULL) {
			exports->stop = strdup(cursym->name);
			continue;
		}
		if (strcmp(cursym->name, "module_exit") == 0 && exports->exit == NULL) {
			exports->exit = strdup(cursym->name);
			continue;
		}

		if (cursym->visibility != STV_DEFAULT)
			continue;

		if (exportlib == NULL) {
			exportlib = calloc(1, sizeof(vita_library_export));

			strLen = strlen(exports->name);
			exportlib->name = strdup(exports->name);
			exportlib->nid = sha256_32_vector(1, (uint8_t **)&exportlib->name, &strLen);
			exportlib->syscall = 0;
			exportlib->version = 1;

			exports->libs = realloc(exports->libs, sizeof(vita_library_export *) * (exports->lib_n + 1));
			exports->libs[exports->lib_n++] = exportlib;
		}

		exportSym = malloc(sizeof(vita_export_symbol));

		strLen = strlen(cursym->name);
		exportSym->name = strdup(cursym->name);
		exportSym->nid = sha256_32_vector(1, (uint8_t **)&exportSym->name, &strLen);

		if (cursym->type == STT_FUNC) {
			exportlib->functions = realloc(exportlib->functions, sizeof(vita_export_symbol *) * (exportlib->function_n + 1));
			exportlib->functions[exportlib->function_n++] = exportSym;
		}
		else {
			exportlib->variables = realloc(exportlib->variables, sizeof(vita_export_symbol *) * (exportlib->variable_n + 1));
			exportlib->variables[exportlib->variable_n++] = exportSym;
		}
	}

	// Sort exports by NID
	qsort(exportlib->variables, exportlib->variable_n, sizeof(vita_export_symbol *), compar_export_symbols);
	qsort(exportlib->functions, exportlib->function_n, sizeof(vita_export_symbol *), compar_export_symbols);
}

typedef vita_imports_stub_t *(*find_stub_func_ptr)(vita_imports_module_t *, uint32_t);
static int lookup_stubs(vita_elf_stub_t *stubs, int num_stubs, find_stub_func_ptr find_stub, const char *stub_type_name)
{
	int found_all = 1;
	int i, j;
	vita_elf_stub_t *stub;

	for (i = 0; i < num_stubs; i++) {
		stub = &(stubs[i]);
		stub->target = vita_imports_stub_new(stub->symbol ? stub->symbol->name : "(unreferenced stub)", 0);
	}

	return found_all;
}

int vita_elf_lookup_imports(vita_elf_t *ve)
{
	int found_all = 1;
	if (!lookup_stubs(ve->fstubs, ve->num_fstubs, (find_stub_func_ptr)&vita_imports_find_function, "function"))
		found_all = 0;
	if (!lookup_stubs(ve->vstubs, ve->num_vstubs, (find_stub_func_ptr)&vita_imports_find_variable, "variable"))
		found_all = 0;

	return found_all;
}

const void *vita_elf_vaddr_to_host(const vita_elf_t *ve, Elf32_Addr vaddr)
{
	vita_elf_segment_info_t *seg;
	int i;

	for (i = 0, seg = ve->segments; i < ve->num_segments; i++, seg++) {
		if (vaddr >= seg->vaddr && vaddr < seg->vaddr + seg->memsz)
			return seg->vaddr_top + vaddr - seg->vaddr;
	}

	return NULL;
}
const void *vita_elf_segoffset_to_host(const vita_elf_t *ve, int segndx, uint32_t offset)
{
	vita_elf_segment_info_t *seg = ve->segments + segndx;

	if (offset < seg->memsz)
		return seg->vaddr_top + offset;

	return NULL;
}

Elf32_Addr vita_elf_host_to_vaddr(const vita_elf_t *ve, const void *host_addr)
{
	vita_elf_segment_info_t *seg;
	int i;

	if (host_addr == NULL)
		return 0;

	for (i = 0, seg = ve->segments; i < ve->num_segments; i++, seg++) {
		if (host_addr >= seg->vaddr_top && host_addr < seg->vaddr_bottom)
			return seg->vaddr + (uint32_t)(host_addr - seg->vaddr_top);
	}

	return 0;
}

int vita_elf_host_to_segndx(const vita_elf_t *ve, const void *host_addr)
{
	vita_elf_segment_info_t *seg;
	int i;

	for (i = 0, seg = ve->segments; i < ve->num_segments; i++, seg++) {
		if (host_addr >= seg->vaddr_top && host_addr < seg->vaddr_bottom)
			return i;
	}

	return -1;
}

int32_t vita_elf_host_to_segoffset(const vita_elf_t *ve, const void *host_addr, int segndx)
{
	vita_elf_segment_info_t *seg = ve->segments + segndx;

	if (host_addr == NULL)
		return 0;

	if (host_addr >= seg->vaddr_top && host_addr < seg->vaddr_bottom)
		return (uint32_t)(host_addr - seg->vaddr_top);

	return -1;
}

int vita_elf_vaddr_to_segndx(const vita_elf_t *ve, Elf32_Addr vaddr)
{
	vita_elf_segment_info_t *seg;
	int i;

	for (i = 0, seg = ve->segments; i < ve->num_segments; i++, seg++) {
		/* Segments of type EXIDX will duplicate '.ARM.extab .ARM.exidx' sections already present in the data segment
		 * Since these won't be loaded, we should prefer the actual data segment */
		if (seg->type == SHT_ARM_EXIDX)
			continue;
		if (vaddr >= seg->vaddr && vaddr < seg->vaddr + seg->memsz)
			return i;
	}

	return -1;
}

/* vita_elf_vaddr_to_segoffset won't check the validity of the address, it may have been fuzzy-matched */
uint32_t vita_elf_vaddr_to_segoffset(const vita_elf_t *ve, Elf32_Addr vaddr, int segndx)
{
	vita_elf_segment_info_t *seg = ve->segments + segndx;

	if (vaddr == 0)
		return 0;

	return vaddr - seg->vaddr;
}
