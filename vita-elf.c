#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>
/* Note:
 * Even though we know the Vita is a 32-bit platform and we specifically check
 * that we're operating on a 32-bit ELF only, we still use the GElf family of
 * functions.  This is because they have extra sanity checking baked in.
 */

#include <endian.h>

#include "vita-elf.h"
#include "vita-import.h"

#define FAIL_EX(label, function, fmt...) do { \
	function(fmt); \
	goto label; \
} while (0)
#define FAIL(fmt...) FAIL_EX(failure, warn, fmt)
#define FAILX(fmt...) FAIL_EX(failure, warnx, fmt)
#define FAILE(fmt, args...) FAIL_EX(failure, warnx, fmt ": %s", ##args, elf_errmsg(-1))
#define ASSERT(condition) do { \
	if (!(condition)) FAILX("Assertion failed: (" #condition ")"); \
} while (0)

static int load_stubs(vita_elf_t *ve, int stubs_ndx, int *num_stubs, vita_elf_stub_t **stubs)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;
	Elf_Data *data;
	uint32_t *stub_data;
	int chunk_offset, total_bytes;
	vita_elf_stub_t *curstub;

	scn = elf_getscn(ve->elf, stubs_ndx);
	gelf_getshdr(scn, &shdr);

	*num_stubs = shdr.sh_size / 16;
	*stubs = calloc(*num_stubs, sizeof(vita_elf_stub_t));

	curstub = *stubs;
	data = NULL; total_bytes = 0;
	while (total_bytes < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {

		for (stub_data = (uint32_t *)data->d_buf, chunk_offset = 0;
				chunk_offset < data->d_size;
				stub_data += 4, chunk_offset += 16) {
			curstub->addr = shdr.sh_addr + data->d_off + chunk_offset;
			curstub->library_nid = le32toh(stub_data[0]);
			curstub->module_nid = le32toh(stub_data[1]);
			curstub->target_nid = le32toh(stub_data[2]);
			curstub++;
		}

		total_bytes += data->d_size;
	}

	return 1;
}

static int lookup_symbols(vita_elf_t *ve, int num_stubs, vita_elf_stub_t *stubs, int symtab_ndx, int stubs_ndx, int sym_type)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	int total_bytes;
	int data_beginsym, symndx;
	const char *sym_name;
	int stub;

	scn = elf_getscn(ve->elf, symtab_ndx);
	gelf_getshdr(scn, &shdr);

	data = NULL; total_bytes = 0;
	while (total_bytes < shdr.sh_size &&
			(data = elf_getdata(scn, data)) != NULL) {

		data_beginsym = data->d_off / shdr.sh_entsize;
		for (symndx = shdr.sh_info > data_beginsym ? shdr.sh_info - data_beginsym : 0;
				symndx < data->d_size / shdr.sh_entsize;
				symndx++) {
			if (gelf_getsym(data, symndx, &sym) != &sym)
				FAILE("gelf_getsym() failed");

			if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL)
				continue;
			if (GELF_ST_TYPE(sym.st_info) != STT_FUNC && GELF_ST_TYPE(sym.st_info) != STT_OBJECT)
				continue;
			if (sym.st_shndx != stubs_ndx)
				continue;

			sym_name = elf_strptr(ve->elf, shdr.sh_link, sym.st_name);
			if (GELF_ST_TYPE(sym.st_info) != sym_type)
				FAILX("Global symbol %s in section %d expected to have type %d; instead has type %d",
						sym_name, sym.st_shndx, sym_type, GELF_ST_TYPE(sym.st_info));

			for (stub = 0; stub < num_stubs; stub++) {
				if (stubs[stub].addr != sym.st_value)
					continue;
				stubs[stub].sym_name = sym_name;
				break;
			}

			if (stub == num_stubs)
				FAILX("Global symbol %s in section %d not pointing to a valid stub",
						sym_name, sym.st_shndx);
		}

		total_bytes += data->d_size;
	}

	return 1;

failure:
	return 0;
}

vita_elf_t *vita_elf_load(const char *filename)
{
	vita_elf_t *ve = NULL;
	GElf_Ehdr ehdr;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	size_t shstrndx;
	char *name;

	if (elf_version(EV_CURRENT) == EV_NONE)
		FAILX("ELF library initialization failed: %s", elf_errmsg(-1));

	ve = calloc(1, sizeof(vita_elf_t));
	ASSERT(ve != NULL);
	ve->fd = -1;

	if ((ve->fd = open(filename, O_RDONLY)) < 0)
		FAIL("open %s failed", filename);

	if ((ve->elf = elf_begin(ve->fd, ELF_C_READ, NULL)) == NULL)
		FAILE("elf_begin() failed");

	if (elf_kind(ve->elf) != ELF_K_ELF)
		FAILX("%s is not an ELF file", filename);

	if (gelf_getehdr(ve->elf, &ehdr) == NULL)
		FAILE("getehdr() failed");

	if (ehdr.e_machine != EM_ARM)
		FAILX("%s is not an ARM binary", filename);

	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 || ehdr.e_ident[EI_DATA] != ELFDATA2LSB)
		FAILX("%s is not a 32-bit, little-endian binary", filename);

	if (elf_getshdrstrndx(ve->elf, &shstrndx) != 0)
		FAILE("elf_getshdrstrndx() failed");

	scn = NULL;

	int symtab_ndx = 0, dynsym_ndx = 0;

	while ((scn = elf_nextscn(ve->elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr)
			FAILE("getshdr() failed");

		if ((name = elf_strptr(ve->elf, shstrndx, shdr.sh_name)) == NULL)
			FAILE("elf_strptr() failed");

		if (shdr.sh_type == SHT_PROGBITS && strcmp(name, ".vitalink.fstubs") == 0) {
			if (ve->fstubs_ndx != 0)
				FAILX("Multiple .vitalink.fstubs sections in binary");
			ve->fstubs_ndx = elf_ndxscn(scn);
		} else if (shdr.sh_type == SHT_PROGBITS && strcmp(name, ".vitalink.vstubs") == 0) {
			if (ve->vstubs_ndx != 0)
				FAILX("Multiple .vitalink.vstubs sections in binary");
			ve->vstubs_ndx = elf_ndxscn(scn);
		}

		if (shdr.sh_type == SHT_SYMTAB)
			symtab_ndx = elf_ndxscn(scn);
		else if (shdr.sh_type == SHT_DYNSYM)
			dynsym_ndx = elf_ndxscn(scn);
	}

	if (ve->fstubs_ndx == 0 && ve->vstubs_ndx == 0)
		FAILX("No .vitalink stub sections in binary, probably not a Vita binary");

	if (ve->fstubs_ndx != 0) {
		if (!load_stubs(ve, ve->fstubs_ndx, &ve->num_fstubs, &ve->fstubs)) goto failure;
		if (!lookup_symbols(ve, ve->num_fstubs, ve->fstubs, symtab_ndx, ve->fstubs_ndx, STT_FUNC)) goto failure;
	}

	if (ve->vstubs_ndx != 0) {
		if (!load_stubs(ve, ve->vstubs_ndx, &ve->num_vstubs, &ve->vstubs)) goto failure;
		if (!lookup_symbols(ve, ve->num_vstubs, ve->vstubs, symtab_ndx, ve->vstubs_ndx, STT_OBJECT)) goto failure;
	}


	return ve;

failure:
	if (ve != NULL)
		vita_elf_free(ve);
	return NULL;
}

void vita_elf_free(vita_elf_t *ve)
{
	/* free() is safe to call on NULL */
	free(ve->fstubs);
	free(ve->vstubs);
	if (ve->elf != NULL)
		elf_end(ve->elf);
	if (ve->fd >= 0)
		close(ve->fd);
	free(ve);
}

typedef vita_imports_stub_t *(*find_stub_func_ptr)(vita_imports_module_t *, uint32_t);
static int lookup_stubs(vita_elf_stub_t *stubs, int num_stubs, vita_imports_t *imports, find_stub_func_ptr find_stub, const char *stub_type_name)
{
	int found_all = 1;
	int i;
	vita_elf_stub_t *stub;

	for (i = 0; i < num_stubs; i++) {
		stub = &(stubs[i]);

		stub->library = vita_imports_find_lib(imports, stub->library_nid);
		if (stub->library == NULL) {
			warnx("Unable to find library with NID %u for %s symbol %s",
					stub->library_nid, stub_type_name,
					stub->sym_name ? stub->sym_name : "(unreferenced stub)");
			found_all = 0;
			continue;
		}

		stub->module = vita_imports_find_module(stub->library, stub->module_nid);
		if (stub->module == NULL) {
			warnx("Unable to find module with NID %u for %s symbol %s",
					stub->module_nid, stub_type_name,
					stub->sym_name ? stub->sym_name : "(unreferenced stub)");
			found_all = 0;
			continue;
		}

		stub->target = find_stub(stub->module, stub->target_nid);
		if (stub->target == NULL) {
			warnx("Unable to find %s with NID %u for symbol %s",
					stub_type_name, stub->module_nid,
					stub->sym_name ? stub->sym_name : "(unreferenced stub)");
			found_all = 0;
		}
	}

	return found_all;
}

int vita_elf_lookup_imports(vita_elf_t *ve, vita_imports_t *imports)
{
	int found_all = 1;

	if (!lookup_stubs(ve->fstubs, ve->num_fstubs, imports, &vita_imports_find_function, "function"))
		found_all = 0;
	if (!lookup_stubs(ve->vstubs, ve->num_vstubs, imports, &vita_imports_find_variable, "variable"))
		found_all = 0;

	return found_all;
}
