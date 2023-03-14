#include "elf-create-argp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int parse_arguments(int argc, char *argv[], elf_create_args *arguments)
{
	int c;
	char *entrypoint_list = NULL;

	arguments->log_level = 0;
	arguments->check_stub_count = 1;
	arguments->is_test_stripping = 0;

	while ((c = getopt(argc, argv, "vne:sg:m:")) != -1)
	{
		switch (c)
		{
		case 'v':
			arguments->log_level++;
			break;
		case 'e':
			arguments->exports = optarg;
			break;
		case 'n':
			arguments->check_stub_count = 0;
			break;
		case 's':
			arguments->is_test_stripping = 1;
			break;
		case 'g':
			arguments->exports_output = optarg;
			break;
		case 'm':
			entrypoint_list = optarg;
			break;
		case '?':
			fprintf(stderr, "unknown option -%c\n", optopt);
			return -1;
		default:
			abort();
		}
	}

	if (argc - optind < 2)
	{
		printf("too few arguments\n");
		return -1;
	}

	if (arguments->exports && entrypoint_list)
	{
		printf("Options -m and -e cannot be used together\n");
		return -1;
	}

	if (arguments->exports && arguments->exports_output)
	{
		printf("Options -g and -e cannot be used together\n");
		return -1;
	}

	if (entrypoint_list)
	{
		char *entrypoint = entrypoint_list;
		char *entrypoint_end = strstr(entrypoint, ",");
		int i = 0;
		while (entrypoint_end != NULL)
		{
			if (i >= 2)
			{
				fprintf(stderr, "list for -m too long\n");
				return -1;
			}
			size_t entrypoint_length = (size_t)(entrypoint_end - entrypoint);
			if (entrypoint_length > 0)
				arguments->entrypoint_funcs[i] = strndup(entrypoint, entrypoint_length);

			i++;
			entrypoint = entrypoint_end + 1;
			entrypoint_end = strstr(entrypoint, ",");
		}

		if (i < 2)
		{
			fprintf(stderr, "list for -m too short\n");
			return -1;
		}

		if (entrypoint[0] != '\0') // If it's a null character then there's no entrypoint here
			arguments->entrypoint_funcs[i] = strdup(entrypoint);
	}

	arguments->input = argv[optind];
	arguments->output = argv[optind + 1];

	if (argc - optind > 2)
	{
		arguments->extra_imports = &argv[optind + 2];
		arguments->extra_imports_count = argc - (optind + 2);
	}

	return 0;
}
