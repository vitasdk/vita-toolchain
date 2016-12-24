#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "import.h"

#define KERNEL_LIBS_STUB "SceKernel"

void usage();
int generate_assembly(vita_imports_t **imports, int imports_count);
int generate_makefile(vita_imports_t **imports, int imports_count);
int generate_cmake(vita_imports_t **imports, int imports_count);

int main(int argc, const char **argv)
{
	int cmake = 0;

	if (argc > 1 && strcmp(argv[1], "-c") == 0) {
		cmake = 1;
		argc--;
		argv++;
	}

	if (argc < 3) {
		usage();
		goto exit_failure;
	}

	int imports_count = argc - 2;

	vita_imports_t **imports = malloc(sizeof(vita_imports_t*) * imports_count);

	int i;
	for (i = 0; i < imports_count; i++)
	{
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

	if (cmake) {
		if (!generate_cmake(imports, imports_count)) {
			fprintf(stderr, "Error generating the assembly makefile\n");
			goto exit_failure;
		}
	} else {
		if (!generate_makefile(imports, imports_count)) {
			fprintf(stderr, "Error generating the assembly makefile\n");
			goto exit_failure;
		}
	}

	for (i = 0; i < imports_count; i++)
	{
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

				for (k = 0; k < module->n_functions; k++) {
					vita_imports_stub_t *function = module->functions[k];
					const char *fname = function->name;
					char filename[4096];
					snprintf(filename, sizeof(filename), "%s_%s_%s.S", library->name, module->name, fname);
					if ((fp = fopen(filename, "w")) == NULL)
						return 0;
					fprintf(fp, ".arch armv7a\n\n");
					fprintf(fp, ".section .vitalink.fstubs.%s,\"ax\",%%progbits\n\n", module->name);
					fprintf(fp,
						"\t.align 4\n"
						"\t.global %s\n"
						"\t.type %s, %%function\n"
						"%s:\n"
						".if GEN_WEAK_EXPORTS\n"
						"\t.word 0x00000008\n"
						".else\n"
						"\t.word 0x00000000\n"
						".endif //GEN_WEAK_EXPORTS\n"
						"\t.word 0x%08X\n"
						"\t.word 0x%08X\n"
						"\t.align 4\n\n",
						fname, fname, fname,
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
					fprintf(fp, ".section .vitalink.vstubs.%s,\"aw\",%%progbits\n\n",module->name);
					fprintf(fp,
						"\t.align 4\n"
						"\t.global %s\n"
						"\t.type %s, %%object\n"
						"%s:\n"
						".if GEN_WEAK_EXPORTS\n"
						"\t.word 0x00000008\n"
						".else\n"
						"\t.word 0x00000000\n"
						".endif //GEN_WEAK_EXPORTS\n"
						"\t.word 0x%08X\n"
						"\t.word 0x%08X\n"
						"\t.align 4\n\n",
						vname, vname, vname,
						module->NID,
						variable->NID);
					fclose(fp);
				}
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
	}
	fprintf(fp, "%s", symbol); // write regardless if its kernel or not
}

void write_cmake_sources(FILE *fp, const char *modname, vita_imports_module_t *library)
{
	int k;

	for (k = 0; k < library->n_functions; k++) {
		vita_imports_stub_t *function = library->functions[k];
		fprintf(fp, "\t\"%s_%s_%s.S\"\n", modname, library->name, function->name);
	}
	for (k = 0; k < library->n_variables; k++) {
		vita_imports_stub_t *variable = library->variables[k];
		fprintf(fp, "\t\"%s_%s_%s.S\"\n", modname, library->name, variable->name);
	}
}

int generate_cmake_user(FILE *fp, vita_imports_lib_t *module)
{
	int i;
	int found_libs = 0;

	for (i = 0; i < module->n_modules; i++)
	{
		vita_imports_module_t *library = module->modules[i];

		// skip kernel
		if (library->is_kernel)
			continue;

		if (!found_libs)
		{
			fprintf(fp, "set(%s_ASM\n", module->name);
			found_libs = 1;
		}

		write_cmake_sources(fp, module->name, library);
	}

	if (found_libs)
	{
		fputs(")\n\n", fp);
	}

	return found_libs;
}

int generate_cmake_kernel(FILE *fp, const char *modname, vita_imports_module_t *library)
{
	if (!library->n_functions && !library->n_variables)
		return 0;

	fprintf(fp, "set(%s_ASM\n", library->name);
	write_cmake_sources(fp, modname, library);
	fputs(")\n\n", fp);

	return 1;
}

int generate_cmake(vita_imports_t **imports, int imports_count)
{
	int h, i, j, k;
	int is_special;

	// TODO: something dynamic
	const char *user_libs[1024];
	int num_user_libs = 0;
	const char *kernel_libs[1024];
	int num_kernel_libs = 0;

	if ((fp = fopen("CMakeLists.txt", "w")) == NULL) {
		return 0;
	}

	fputs(
		"cmake_minimum_required(VERSION 2.8)\n\n"
		"if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)\n"
		"\tif(DEFINED ENV{VITASDK})\n"
		"\t\tset(CMAKE_TOOLCHAIN_FILE \"$ENV{VITASDK}/share/vita.toolchain.cmake\" CACHE PATH \"toolchain file\")\n"
		"\telse()\n"
		"\t\tmessage(FATAL_ERROR \"Please define VITASDK to point to your SDK path!\")\n"
		"\tendif()\n"
		"endif()\n"
		"project(vitalibs)\n"
		"enable_language(ASM)\n\n", fp);

	for (h = 0; h < imports_count; ++h)
	{
		vita_imports_t *imp = imports[h];

		for (i = 0; i < imp->n_libs; i++)
		{
			vita_imports_lib_t *module = imp->libs[i];

			// generate user libs first
			if (generate_cmake_user(fp, module))
			{
				user_libs[num_user_libs++] = module->name;
			}

			for (j = 0; j < imp->libs[i]->n_modules; j++)
			{
				vita_imports_module_t *library = imp->libs[i]->modules[j];

				if (!library->is_kernel)
					continue;

				if (generate_cmake_kernel(fp, module->name, library))
				{
					kernel_libs[num_kernel_libs++] = library->name;
				}
			}
		}
	}

	if (num_user_libs > 0)
	{
		fputs("set(USER_LIBRARIES\n", fp);

		for (i = 0; i < num_user_libs; ++i)
		{
			fprintf(fp, "\t\"%s\"\n", user_libs[i]);
		}

		fputs(")\n\n", fp);
	}

	if (num_kernel_libs > 0)
	{
		fputs("set(KERNEL_LIBRARIES\n", fp);

		for (i = 0; i < num_kernel_libs; ++i)
		{
			fprintf(fp, "\t\"%s\"\n", kernel_libs[i]);
		}

		fputs(")\n\n", fp);
	}

	if (num_user_libs > 0)
	{
		fputs(
			"foreach(library ${USER_LIBRARIES})\n"
			"\tadd_library(${library}_stub STATIC ${${library}_ASM})\n"
			"\ttarget_compile_definitions(${library}_stub PRIVATE -DGEN_WEAK_EXPORTS=0)\n"
			"\tadd_library(${library}_stub_weak STATIC ${${library}_ASM})\n"
			"\ttarget_compile_definitions(${library}_stub_weak PRIVATE -DGEN_WEAK_EXPORTS=1)\n"
			"endforeach(library)\n\n", fp);
	}

	if (num_kernel_libs > 0)
	{
		fputs(
			"foreach(library ${KERNEL_LIBRARIES})\n"
			"\tadd_library(${library}_stub STATIC ${${library}_ASM})\n"
			"\ttarget_compile_definitions(${library}_stub PRIVATE -DGEN_WEAK_EXPORTS=0)\n"
			"endforeach(library)\n\n", fp);
	}

	fclose(fp);
	return 1;
}

int generate_makefile(vita_imports_t **imports, int imports_count)
{
	int h, i, j, k;
	int is_special;

	if ((fp = fopen("Makefile", "w")) == NULL) {
		return 0;
	}

	g_special_size = 1024;
	g_special_written = 0;
	g_kernel_objs = malloc(g_special_size);
	g_kernel_objs[0] = '\0';

	fputs(
		"ifdef VITASDK\n"
		"PREFIX = $(VITASDK)/bin/\n"
		"endif\n\n"
		"ARCH ?= $(PREFIX)arm-vita-eabi\n"
		"AS = $(ARCH)-as\n"
		"AR = $(ARCH)-ar\n"
		"RANLIB = $(ARCH)-ranlib\n\n"
		"TARGETS =", fp);

	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			fprintf(fp, " lib%s_stub.a", imp->libs[i]->name);

			for (j = 0; j < imp->libs[i]->n_modules; j++) {
				vita_imports_module_t *module = imp->libs[i]->modules[j];

				if (!module->is_kernel)
					continue;

				fprintf(fp, " lib%s_stub.a", module->name);
			}
		}
	}

	fprintf(fp, "\nTARGETS_WEAK =");
	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			fprintf(fp, " lib%s_stub_weak.a", imp->libs[i]->name);
		}
	}

	fprintf(fp, "\n\n");

	for (h = 0; h < imports_count; h++) {
		vita_imports_t *imp = imports[h];
		for (i = 0; i < imp->n_libs; i++) {
			vita_imports_lib_t *library = imp->libs[i];
			is_special = (strcmp(KERNEL_LIBS_STUB, library->name) == 0);

			for (int weak = 0; weak < 2; weak++) {
				if (!is_special) {
					fprintf(fp, "%s%s =", library->name, weak ? "_weak_OBJS" : "_OBJS");
				}

				for (j = 0; j < library->n_modules; j++) {
					vita_imports_module_t *module = library->modules[j];

					if(module->is_kernel)
						continue;

					char buf[4096];
					for (k = 0; k < module->n_functions; k++) {
						vita_imports_stub_t *function = module->functions[k];
						snprintf(buf, sizeof(buf), " %s_%s_%s.%s", library->name, module->name, function->name, weak ? "wo" : "o");
						write_symbol(buf, is_special);
					}
					for (k = 0; k < module->n_variables; k++) {
						vita_imports_stub_t *variable = module->variables[k];
						snprintf(buf, sizeof(buf), " %s_%s_%s.%s", library->name, module->name, variable->name, weak ? "wo" : "o");
						write_symbol(buf, is_special);
					}
				}

				if (!is_special) {
					fprintf(fp, "\n");
				}
			}

			for (j = 0; j < library->n_modules; j++) {
				vita_imports_module_t *module = library->modules[j];

				if (!module->is_kernel)
					continue;

				char buf[4096];

				fprintf(fp, "%s_OBJS =", module->name);

				for (k = 0; k < module->n_functions; k++) {
					vita_imports_stub_t *function = module->functions[k];
					snprintf(buf, sizeof(buf), " %s_%s_%s.o", library->name, module->name, function->name);
					write_symbol(buf, 1);
				}

				for (k = 0; k < module->n_variables; k++) {
					vita_imports_stub_t *variable = module->variables[k];
					snprintf(buf, sizeof(buf), " %s_%s_%s.o", library->name, module->name, variable->name);
					write_symbol(buf, 1);
				}

				fprintf(fp, "\n");
			}
		}
	}

	// write kernel lib stub
	fprintf(fp, "%s_OBJS =%s\n", KERNEL_LIBS_STUB, g_kernel_objs);

	fputs(
		"ALL_OBJS=\n\n"
		"all: $(TARGETS) $(TARGETS_WEAK)\n\n"
		"define LIBRARY_template\n"
		" $(1): $$($(1:lib%_stub.a=%)_OBJS)\n"
		" ALL_OBJS += $$($(1:lib%_stub.a=%)_OBJS)\n"
		"endef\n"
		"define LIBRARY_WEAK_template\n"
		" $(1): $$($(1:lib%_stub_weak.a=%)_weak_OBJS)\n"
		" ALL_OBJS += $$($(1:lib%_stub_weak.a=%)_weak_OBJS)\n"
		"endef\n\n"
		"$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))\n"
		"$(foreach library,$(TARGETS_WEAK),$(eval $(call LIBRARY_WEAK_template,$(library))))\n\n"
		"install: $(TARGETS) $(TARGETS_WEAK)\n"
		"\tcp $(TARGETS) $(VITASDK)/arm-vita-eabi/lib\n"
		"\tcp $(TARGETS_WEAK) $(VITASDK)/arm-vita-eabi/lib\n\n"
		"clean:\n"
		"\trm -f $(TARGETS) $(TARGETS_WEAK) $(ALL_OBJS)\n\n"
		"$(TARGETS) $(TARGETS_WEAK):\n"
		"\t$(AR) cru $@ $?\n"
		"\t$(RANLIB) $@\n\n"
		"%.o: %.S\n"
		"\t$(AS) --defsym GEN_WEAK_EXPORTS=0 $< -o $@\n\n"
		"%.wo: %.S\n"
		"\t$(AS) --defsym GEN_WEAK_EXPORTS=1 $< -o $@\n"
		, fp);

	fclose(fp);
	free(g_kernel_objs);

	return 1;
}

void usage()
{
	fprintf(stderr,
		"vita-libs-gen by xerpi\n"
		"Usage: vita-libs-gen [-c] nids.yml [extra.yml ...] output-dir\n"
		"\t-c: Generate CMakeLists.txt instead of a Makefile\n"
	);
}
