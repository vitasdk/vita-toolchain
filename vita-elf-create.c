#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"
#include "vita-import.h"

void print_stubs(vita_elf_stub_t *stubs, int num_stubs)
{
	int i;

	for (i = 0; i < num_stubs; i++) {
		printf("  0x%06x (%s):\n", stubs[i].addr, stubs[i].symbol ? stubs[i].symbol->name : "unreferenced stub");
		printf("    Library: %u (%s)\n", stubs[i].library_nid, stubs[i].library ? stubs[i].library->name : "not found");
		printf("    Module : %u (%s)\n", stubs[i].module_nid, stubs[i].module ? stubs[i].module->name : "not found");
		printf("    NID    : %u (%s)\n", stubs[i].target_nid, stubs[i].target ? stubs[i].target->name : "not found");
	}
}

#define R_ARM_THM_CALL R_ARM_THM_PC22

const char *get_scn_name(vita_elf_t *ve, Elf_Scn *scn)
{
	size_t shstrndx;
	GElf_Shdr shdr;

	elf_getshdrstrndx(ve->elf, &shstrndx);
	gelf_getshdr(scn, &shdr);
	return elf_strptr(ve->elf, shstrndx, shdr.sh_name);
}

const char *get_scndx_name(vita_elf_t *ve, int scndx)
{
	return get_scn_name(ve, elf_getscn(ve->elf, scndx));
}

const char *get_rel_type_name(int type)
{
	switch(type) {
#define R_ARM(type) case R_ARM_##type: return "R_ARM_" #type
		R_ARM(NONE);
		R_ARM(V4BX);
		R_ARM(ABS32);
		R_ARM(REL32);
		R_ARM(THM_CALL);
		R_ARM(CALL);
		R_ARM(JUMP24);
		R_ARM(TARGET1);
		R_ARM(TARGET2);
		R_ARM(PREL31);
		R_ARM(MOVW_ABS_NC);
		R_ARM(MOVT_ABS);
		R_ARM(THM_MOVW_ABS_NC);
		R_ARM(THM_MOVT_ABS);
#undef R_ARM
	}

	return "<<INVALID RELOCATION>>";
}

#define THUMB_SHUFFLE(x) ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))
uint32_t get_rel_target(uint32_t data, int type, uint32_t addr)
{
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
			return (((((data >> 16) & 0x7ff) << 11)
				| ((data & 0x7ff))) << 1) + addr + 4;
		case R_ARM_CALL: // bl/blx
		case R_ARM_JUMP24: // b/bl<cond>
			return ((data & 0x00ffffff) << 2) + addr + 8;
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

void list_rels(vita_elf_t *ve)
{
	Elf_Scn *scn, *text_scn, *sym_scn;
	GElf_Shdr shdr, text_shdr, sym_shdr;
	GElf_Rel rel;
	Elf_Data *data, *text_data, *sym_data;
	GElf_Sym sym;
	int relndx;
	struct {
		int type;
		int expect;
		int sym;
		uint32_t offset;
		uint32_t target;
	} movw_rel = {0};
	int rel_type, rel_sym;
	uint32_t target;
	const char *sym_name;

	scn = NULL;

	while ((scn = elf_nextscn(ve->elf, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);

		if (shdr.sh_type == SHT_REL) {
			printf("REL section %zd: %s (symtab %d: %s, target %d: %s):\n",
					elf_ndxscn(scn), get_scn_name(ve, scn),
					shdr.sh_link, get_scndx_name(ve, shdr.sh_link),
					shdr.sh_info, get_scndx_name(ve, shdr.sh_info));
			text_scn = elf_getscn(ve->elf, shdr.sh_info);
			gelf_getshdr(text_scn, &text_shdr);
			text_data = elf_getdata(text_scn, NULL);

			sym_scn = elf_getscn(ve->elf, shdr.sh_link);
			gelf_getshdr(sym_scn, &sym_shdr);
			sym_data = elf_getdata(sym_scn, NULL);

			data = elf_getdata(scn, NULL);
			for (relndx = 0; relndx < data->d_size / shdr.sh_entsize; relndx++) {
				if (gelf_getrel(data, relndx, &rel) != &rel)
					errx(EXIT_FAILURE,"gelf_getrel() failed");

				rel_sym = GELF_R_SYM(rel.r_info);
				rel_type = GELF_R_TYPE(rel.r_info);
				if (rel_type == R_ARM_NONE || rel_type == R_ARM_V4BX)
					continue;
				gelf_getsym(sym_data, rel_sym, &sym);
				sym_name = elf_strptr(ve->elf, sym_shdr.sh_link, sym.st_name);

				target = get_rel_target(
						le32toh(*((uint32_t*)(text_data->d_buf+(rel.r_offset - text_shdr.sh_addr)))),
						rel_type, rel.r_offset);

				printf("  offset %06lx: sym %d (%s), type %s\n",
						rel.r_offset, rel_sym,
						sym_name, get_rel_type_name(rel_type));

				if (movw_rel.expect != 0) {
					if (rel_type != movw_rel.expect)
						errx(EXIT_FAILURE,"Expected %s relocation to follow %s!",
								get_rel_type_name(movw_rel.expect), get_rel_type_name(movw_rel.type));
					if (rel_sym != movw_rel.sym)
						errx(EXIT_FAILURE,"Paired MOVW/MOVT relocations do not reference same symbol!");
					if (rel.r_offset != movw_rel.offset + 4)
						errx(EXIT_FAILURE,"Paired MOVW/MOVT relocation not adjacent!");
					target |= movw_rel.target;
					movw_rel.expect = 0;
				} else {
					if (rel_type == R_ARM_MOVT_ABS || rel_type == R_ARM_THM_MOVT_ABS)
						errx(EXIT_FAILURE,"Encountered unexpected %s relocation!",get_rel_type_name(rel_type));
					if (rel_type == R_ARM_MOVW_ABS_NC || rel_type == R_ARM_THM_MOVW_ABS_NC) {
						movw_rel.type = rel_type;
						movw_rel.expect = (rel_type == R_ARM_MOVW_ABS_NC ? R_ARM_MOVT_ABS : R_ARM_THM_MOVT_ABS);
						movw_rel.sym = rel_sym;
						movw_rel.offset = rel.r_offset;
						movw_rel.target = target;
						continue;
					}
				}

				printf("    curval: 0x%08x (%s%+d)\n", target, sym_name, (int32_t)(target - sym.st_value));

			}
		}
	}
}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;
	vita_imports_t *imports;
	int status = EXIT_SUCCESS;

	if (argc < 4)
		errx(EXIT_FAILURE,"Usage: vita-elf-create input-elf output-elf db.json");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	if ((imports = vita_imports_load(argv[3], 0)) == NULL)
		return EXIT_FAILURE;

	if (!vita_elf_lookup_imports(ve, imports))
		status = EXIT_FAILURE;

	if (ve->fstubs_ndx) {
		printf("Function stubs in section %d:\n", ve->fstubs_ndx);
		print_stubs(ve->fstubs, ve->num_fstubs);
	}
	if (ve->vstubs_ndx) {
		printf("Variable stubs in section %d:\n", ve->vstubs_ndx);
		print_stubs(ve->vstubs, ve->num_vstubs);
	}

	printf("Relocations:\n");
	list_rels(ve);

	vita_elf_free(ve);
	vita_imports_free(imports);

	return status;
}
