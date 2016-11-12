#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "vita-import.h"

void usage();
int generate_assembly(vita_imports_t **imports, int imports_count);
int generate_makefile(vita_imports_t **imports, int imports_count);

int main(int argc, char *argv[])
{
	if (argc < 3) {
		usage();
		goto exit_failure;
	}

	int imports_count = argc - 2;

	vita_imports_t **imports = malloc(sizeof(vita_imports_t*) * imports_count);

	int i;
	for (i = 0; i < imports_count; i++) {
		vita_imports_t *imp = vita_imports_load(argv[i + 1], 1);

		if (imp == NULL) {
			goto exit_failure;
		}

		imports[i] = imp;
	}

#if defined(_WIN32) && !defined(__CYGWIN__)
	mkdir(argv[argc - 1]);
#else
	mkdir(argv[argc - 1], 0777); // create directory if it doesn't exist
#endif

	if (chdir(argv[argc - 1])) {
		perror(argv[argc - 1]);
		goto exit_failure;
	}

	if (!generate_assembly(imports, imports_count)) {
		fprintf(stderr, "Error generating the assembly file\n");
		goto exit_failure;
	}

	if (!generate_makefile(imports, imports_count)) {
		fprintf(stderr, "Error generating the Makefile\n");
		goto exit_failure;
	}

	for (i = 0; i < imports_count; i++) {
		vita_imports_free(imports[i]);
	}

	free(imports);

	return EXIT_SUCCESS;
exit_failure:
	return EXIT_FAILURE;
}


int generate_assembly(vita_imports_t **imports, int imports_count)
{
	char filename[128];
	FILE *fp;
	int h, i, j, k;

	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_t *library = imp->libs[i];
			for (j = 0; j < library->n_modules; j++) {
				vita_imports_module_t *module = library->modules[j];

				char filename[4096];
				snprintf(filename, sizeof(filename), "%s_%s.S", library->name, module->name);
				if ((fp = fopen(filename, "w")) == NULL)
					return 0;

				fprintf(fp, ".arch armv7a\n\n");

				if (module->n_functions > 0) {
					fprintf(fp, ".section .vitalink.fstubs.%s,\"ax\",%%progbits\n\n", module->name);

					for (k = 0; k < module->n_functions; k++) {
						vita_imports_stub_t *function = module->functions[k];
						const char *fname = function->name;

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
					}
				}

				if (module->n_variables > 0) {
					fprintf(fp, ".section .vitalink.vstubs.%s,\"aw\",%%progbits\n\n",module->name);

					for (k = 0; k < module->n_variables; k++) {
						vita_imports_stub_t *variable = module->variables[k];
						const char *vname = variable->name;

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
					}
				}

				fclose(fp);
			}
		}
	}

	return 1;
}

int generate_makefile(vita_imports_t **imports, int imports_count)
{
	int h, i, j, k;
	FILE *fp;

	if ((fp = fopen("Makefile", "w")) == NULL) {
		return 0;
	}

	fputs(
		"ARCH   ?= arm-vita-eabi\n"
		"AS      = $(ARCH)-as\n"
		"AR      = $(ARCH)-ar\n"
		"RANLIB  = $(ARCH)-ranlib\n\n"
		"TARGETS =", fp);

	/*
	 * Stubs naming scheme:
	 *     * User libs: libSceFoo_stub.a <- {SceFoo_SceBar0.o, SceFoo_SceBar1.o, ...}
	 *     * Kernel libs: libSceFoo_kernel_stub.a <- {SceFoo_SceBar2.o, SceFoo_SceBar3.o, ...}
	 */

	/*
	 * Stubs libs
	 */
	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_t *library = imp->libs[i];
			int num_user_modules = 0;
			int num_kernel_modules = 0;

			for (j = 0; j < library->n_modules; j++) {
				if (library->modules[j]->is_kernel)
					num_kernel_modules++;
				else
					num_user_modules++;
			}

			if (num_user_modules > 0)
				fprintf(fp, " lib%s_stub.a", imp->libs[i]->name);

			if (num_kernel_modules > 0)
				fprintf(fp, " lib%s_kernel_stub.a", imp->libs[i]->name);
		}
	}

	fprintf(fp, "\n\n# User stubs\n");

	/*
	 * Generate user stubs rules
	 */

	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_t *library = imp->libs[i];
			int found_user_module = 0;

			for (j = 0; j < library->n_modules; j++) {
				vita_imports_module_t *module = library->modules[j];

				if (!module->is_kernel) {
					if (!found_user_module) {
						fprintf(fp, "%s_OBJS =", library->name);
						found_user_module = 1;
					}

					fprintf(fp, " %s_%s.o", library->name, module->name);
				}
			}

			if (found_user_module)
				fprintf(fp, "\n");
		}
	}

	fprintf(fp, "\n# Kernel stubs\n");

	/*
	 * Generate kernel stubs rules
	 */

	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_t *library = imp->libs[i];
			int found_kernel_module = 0;

			for (j = 0; j < library->n_modules; j++) {
				vita_imports_module_t *module = library->modules[j];

				if (module->is_kernel) {
					if (!found_kernel_module) {
						fprintf(fp, "%s_kernel_OBJS =", library->name);
						found_kernel_module = 1;
					}

					fprintf(fp, " %s_%s.o", library->name, module->name);
				}
			}

			if (found_kernel_module)
				fprintf(fp, "\n");
		}
	}

	fprintf(fp, "\n");

	fputs(	"\n"
		"ALL_OBJS=\n\n"
		"all: $(TARGETS)\n\n"
		"define LIBRARY_template\n"
		" $(1): $$($(1:lib%_stub.a=%)_OBJS)\n"
		" ALL_OBJS += $$($(1:lib%_stub.a=%)_OBJS)\n"
		"endef\n\n"
		"$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))\n\n"
		"all: $(TARGETS)\n\n"
		"clean:\n"
		"\trm -f $(TARGETS) $(ALL_OBJS)\n\n"
		"$(TARGETS):\n"
		"\t$(AR) cru $@ $?\n"
		"\t$(RANLIB) $@\n\n"
		"%.o: %.S\n"
		"\t$(AS) $< -o $@\n"
		, fp);

	fclose(fp);

	return 1;
}

void usage()
{
	fprintf(stderr,
		"vita-libs-gen by xerpi\n"
		"usage:\n\tvita-libs-gen nids.json [extra.json ...] output-dir\n"
	);
}
