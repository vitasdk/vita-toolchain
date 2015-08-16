#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "vita-import.h"

#define KERNEL_LIBS_STUB "SceKernel"

void usage();
int generate_assembly(vita_imports_t *imports);
int generate_makefile(vita_imports_t *imports);

int main(int argc, char *argv[])
{
	if (argc < 3) {
		usage();
		goto exit_failure;
	}

	vita_imports_t *imports = vita_imports_load(argv[1], 1);

	if (imports == NULL) {
		goto exit_failure;
	}

	if (chdir(argv[2])) {
		perror(argv[2]);
		goto exit_failure;
	}

	if (!generate_assembly(imports)) {
		fprintf(stderr, "Error generating the assembly file\n");
		goto exit_failure;
	}

	if (!generate_makefile(imports)) {
		fprintf(stderr, "Error generating the assembly makefile\n");
		goto exit_failure;
	}

	vita_imports_free(imports);

	return EXIT_SUCCESS;
exit_failure:
	return EXIT_FAILURE;
}


int generate_assembly(vita_imports_t *imports)
{
	char filename[128];
	FILE *fp;
	int i, j, k;

	for (i = 0; i < imports->n_libs; i++) {
		vita_imports_lib_t *library = imports->libs[i];
		for (j = 0; j < library->n_modules; j++) {
			vita_imports_module_t *module = library->modules[j];

			for (k = 0; k < module->n_functions; k++) {
				vita_imports_stub_t *function = module->functions[k];
				const char *fname = function->name;
				char filename[4096];
				snprintf(filename, sizeof(filename), "%s_%s_%s.S", library->name, module->name, fname);
				if ((fp = fopen(filename, "w")) == NULL)
					return 0;
				fprintf(fp, ".arch armv7a\n\n");
				fprintf(fp, ".section .vitalink.fstubs,\"ax\",%%progbits\n\n");
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%function\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.align 4\n\n",
					fname, fname, fname,
					library->NID,
					module->NID,
					function->NID);
				fclose(fp);
			}

			for (k = 0; k < module->n_variables; k++) {
				vita_imports_stub_t *variable = module->variables[k];
				const char *vname = variable->name;
				char filename[4096];
				snprintf(filename, sizeof(filename), "%s_%s_%s.S", library->name, module->name, vname);
				if ((fp = fopen(filename, "w")) == NULL)
					return 0;
				fprintf(fp, ".arch armv7a\n\n");
				fprintf(fp, ".section .vitalink.vstubs,\"aw\",%%progbits\n\n");
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%object\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.align 4\n\n",
					vname, vname, vname,
					library->NID,
					module->NID,
					variable->NID);
				fclose(fp);
			}
		}
	}

	return 1;
}

char *g_kernel_objs;
size_t g_special_size, g_special_written;
FILE *fp;

void write_symbol(const char *symbol, int is_kernel)
{
	if (is_kernel) {
		size_t len = strlen(symbol);
		while (g_special_written + len >= g_special_size) {
			g_special_size *= 2;
			g_kernel_objs = realloc(g_kernel_objs, g_special_size);
		}
		strcat(g_kernel_objs, symbol);
		g_special_written += len;
	} else {
		fprintf(fp, "%s", symbol);
	}
}

int generate_makefile(vita_imports_t *imports)
{
	int i, j, k;
	int is_special;

	if ((fp = fopen("Makefile", "w")) == NULL) {
		return 0;
	}

	g_special_size = 1024;
	g_special_written = 0;
	g_kernel_objs = malloc(g_special_size);
	g_kernel_objs[0] = '\0';

	fputs(
		"ARCH ?= arm-none-eabi\n"
		"AS = $(ARCH)-as\n"
		"AR = $(ARCH)-ar\n"
		"RANLIB = $(ARCH)-ranlib\n\n"
		"TARGETS =", fp);

	for (i = 0; i < imports->n_libs; i++) {
		fprintf(fp, " lib%s_stub.a", imports->libs[i]->name);
	}

	fprintf(fp, "\n\n");

	for (i = 0; i < imports->n_libs; i++) {
		vita_imports_lib_t *library = imports->libs[i];
		is_special = (strcmp(KERNEL_LIBS_STUB, library->name) == 0);

		if (!is_special) {
			fprintf(fp, "%s_OBJS =", library->name);
		}

		for (j = 0; j < library->n_modules; j++) {
			vita_imports_module_t *module = library->modules[j];
			char buf[4096];
			for (k = 0; k < module->n_functions; k++) {
				vita_imports_stub_t *function = module->functions[k];
				snprintf(buf, sizeof(buf), " %s_%s_%s.o", library->name, module->name, function->name);
				write_symbol(buf, is_special || module->is_kernel);
			}
			for (k = 0; k < module->n_variables; k++) {
				vita_imports_stub_t *variable = module->variables[k];
				snprintf(buf, sizeof(buf), " %s_%s_%s.o", library->name, module->name, variable->name);
				write_symbol(buf, is_special || module->is_kernel);
			}
		}

		if (!is_special) {
			fprintf(fp, "\n");
		}
	}

	// write kernel lib stub
	fprintf(fp, "%s_OBJS =%s\n", KERNEL_LIBS_STUB, g_kernel_objs);

	fputs(
		"ALL_OBJS=\n\n"
		"all: $(TARGETS)\n\n"
		"define LIBRARY_template\n"
		" $(1): $$($(1:lib%_stub.a=%)_OBJS)\n"
		" ALL_OBJS += $$($(1:lib%_stub.a=%)_OBJS)\n"
		"endef\n\n"
		"$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))\n\n"
		"all: $(TARGETS)\n\n"
		"clean:\n\n"
		"\trm -f $(TARGETS) $(ALL_OBJS)\n\n"
		"$(TARGETS):\n"
		"\t$(AR) cru $@ $?\n"
		"\t$(RANLIB) $@\n\n"
		"%.o: %.S\n"
		"\t$(AS) $< -o $@\n"
		, fp);

	fclose(fp);
	free(g_kernel_objs);

	return 1;
}

void usage()
{
	fprintf(stderr,
		"vita-libs-gen by xerpi\n"
		"usage:\n\tvita-libs-gen nids.json output-dir\n"
	);
}
