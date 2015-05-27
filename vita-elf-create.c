#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"

void print_stubs(vita_elf_t *ve, int scndx)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;
	Elf_Data *data;
	uint32_t *p;
	int n, m;

	scn = elf_getscn(ve->elf, scndx);
	gelf_getshdr(scn, &shdr);

	data = NULL; n = 0;
	while (n < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {
		printf("Got data part %d, type %d, size %zd, offset %ld, align %zd, ptr %p\n",n,data->d_type,data->d_size,data->d_off,data->d_align,data->d_buf);
		for (p = data->d_buf, m = 0; m < data->d_size / 16; p += 4, m++) {
			printf("Stub at 0x%06lx:\n", shdr.sh_addr+data->d_off+(m*16));
			printf("  Library: %u\n", le32toh(p[0]));
			printf("  Module : %u\n", le32toh(p[1]));
			printf("  NID    : %u\n", le32toh(p[2]));
		}
		n += data->d_size;
	}
}

const char *bind_str(int bind) {
	switch (bind) {
#define STB(name) case STB_##name: return #name
		STB(LOCAL);
                STB(GLOBAL);
                STB(WEAK);
                STB(NUM);
		case STB_LOOS: return "LOOS/GNU_UNIQUE";
                STB(HIOS);
                STB(LOPROC);
                STB(HIPROC);
#undef STB
	}
	return "(unknown)";
}

const char *type_str(int type) {
	switch (type) {
#define STT(name) case STT_##name: return #name
		STT(NOTYPE);
		STT(OBJECT);
		STT(FUNC);
		STT(SECTION);
		STT(FILE);
		STT(COMMON);
		STT(TLS);
		STT(NUM);
		case STT_LOOS: return "LOOS/GNU_IFUNC";
		STT(HIOS);
		STT(LOPROC);
		STT(HIPROC);
#undef STT
	}
	return "(unknown)";
}

const char *vis_str(int vis) {
	switch (vis) {
#define STV(name) case STV_##name: return #name
		STV(DEFAULT);
		STV(INTERNAL);
		STV(HIDDEN);
		STV(PROTECTED);
#undef STV
	}
	return "(unknown)";
}

void print_symtab(vita_elf_t *ve, int scndx)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	int n;
	int data_beginsym, symndx;
	size_t shstrndx;

	elf_getshdrstrndx(ve->elf, &shstrndx);
	scn = elf_getscn(ve->elf, scndx);
	gelf_getshdr(scn, &shdr);

	printf("Symbol table at idx %d, name %s, strndx %d, begin globals at %d:\n", scndx, elf_strptr(ve->elf, shstrndx, shdr.sh_name), shdr.sh_link, shdr.sh_info);

	data = NULL; n = 0;
	while (n < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {
		data_beginsym = data->d_off / shdr.sh_entsize;
		for (symndx = shdr.sh_info > data_beginsym ? shdr.sh_info - data_beginsym : 0; symndx < data->d_size / shdr.sh_entsize; symndx++) {
			if (gelf_getsym(data, symndx, &sym) != &sym)
				errx(EXIT_FAILURE,"gelf_getsym() failed: %s", elf_errmsg(-1));
			if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL ||
					(GELF_ST_TYPE(sym.st_info) != STT_FUNC && GELF_ST_TYPE(sym.st_info) != STT_OBJECT))
				continue;
			printf("  %2d: %-30s bind %-6s type %-8s vis %-8s shndx %d value 0x%06lx size %ld\n",
					data_beginsym + symndx, elf_strptr(ve->elf, shdr.sh_link, sym.st_name),
					bind_str(GELF_ST_BIND(sym.st_info)), type_str(GELF_ST_TYPE(sym.st_info)),
					vis_str(GELF_ST_VISIBILITY(sym.st_other)),
					sym.st_shndx, sym.st_value, sym.st_size);
		}
		n += data->d_size;
	}

}

int main(int argc, char *argv[])
{
	vita_elf_t *ve;

	if (argc < 3)
		errx(EXIT_FAILURE,"Usage: vita-elf-create input-elf output-elf");

	if ((ve = vita_elf_load(argv[1])) == NULL)
		return EXIT_FAILURE;

	fprintf(stderr,"Got fstubs in section %d, vstubs in section %d\n", ve->fstubs_ndx, ve->vstubs_ndx);

	printf("fstubs:\n");
	print_stubs(ve, ve->fstubs_ndx);

	printf("vstubs:\n");
	print_stubs(ve, ve->vstubs_ndx);

	if (ve->symtab_ndx) {
		printf("symbols:\n");
		print_symtab(ve, ve->symtab_ndx);
	}

	if (ve->dynsym_ndx) {
		printf("dynsym:\n");
		print_symtab(ve, ve->dynsym_ndx);
	}

	vita_elf_free(ve);

	return EXIT_SUCCESS;
}
