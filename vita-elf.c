#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include <libelf.h>
#include <gelf.h>

#include "vita-elf.h"

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

	while ((scn = elf_nextscn(ve->elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr)
			FAILE("getshdr() failed");

		if ((name = elf_strptr(ve->elf, shstrndx, shdr.sh_name)) == NULL)
			FAILE("elf_strptr() failed");

		if (strcmp(name, ".vitalink.fstubs") == 0) {
			if (ve->fstubs_ndx != 0)
				FAILX("Multiple .vitalink.fstubs sections in binary");
			ve->fstubs_ndx = elf_ndxscn(scn);
		}

		if (strcmp(name, ".vitalink.vstubs") == 0) {
			if (ve->vstubs_ndx != 0)
				FAILX("Multiple .vitalink.vstubs sections in binary");
			ve->vstubs_ndx = elf_ndxscn(scn);
		}
	}

	if (ve->fstubs_ndx == 0 && ve->vstubs_ndx == 0)
		FAILX("No .vitalink stub sections in binary, probably not a Vita binary");


	return ve;

failure:
	if (ve != NULL) {
		if (ve->elf != NULL) {
			elf_end(ve->elf);
		}
		if (ve->fd >= 0) {
			close(ve->fd);
		}
		free(ve);
	}
	return NULL;
}

void vita_elf_free(vita_elf_t *ve)
{
	elf_end(ve->elf);
	close(ve->fd);
	free(ve);
}
