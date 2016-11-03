#include "elf-create-argp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int parse_arguments(int argc, char *argv[], elf_create_args *arguments)
{
	int c;

	arguments->log_level = 0;
	arguments->check_stub_count = 1;

	while ((c = getopt(argc, argv, "vne:")) != -1)
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
	
	arguments->input = argv[optind];
	arguments->output = argv[optind+1];
	
	if (argc - optind > 2)
	{
		arguments->extra_imports = &argv[optind+2];
		arguments->extra_imports_count = argc-(optind+2);
	}
	
	return 0;
}
