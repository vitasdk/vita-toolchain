#ifndef ELF_CREATE_ARGP_H
#define ELF_CREATE_ARGP_H

typedef struct elf_create_args
{
	int log_level;
	const char *exports;
	const char *input;
	const char *output;
	int extra_imports_count;
	char **extra_imports;
	int check_stub_count;
} elf_create_args;


int parse_arguments(int argc, char *argv[], elf_create_args *arguments);

#endif // ELF_CREATE_ARGP_H
