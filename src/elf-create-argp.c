#include "elf-create-argp.h"

#include <stdlib.h>
#include <errno.h>
#include <argp.h>

// TODO: do we start versioning this?
const char *argp_program_version = "vita-elf-create";
const char *argp_program_bug_address = "https://github.com/vitasdk/vita-toolchain";

// TODO: improve this shit
static char doc[] = "Convert an executable ELF linked with libraries from vita-libs-gen and produce a SCE ELF.";

// TODO: is this backward compatible?
static char args_doc[] = "input-elf output-elf [extra.json]...";

static struct argp_option options[] = {
	{ NULL, 'v', "v", OPTION_ARG_OPTIONAL, "Enable verbose output."},
	{ "exports", 'e', "exports yaml file", 0, "A YAML file describing the exports"},
	{ 0 }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct elf_create_args *arguments = state->input;
	switch (key) {
	case 'v': {
		int level = 1;

		if (arg && arg[0] == 'v')
			++level;

		arguments->log_level = level;
		break;
	}

	case 'e': {
		arguments->exports = arg;
		break;
	}
	case ARGP_KEY_ARG: {
		if (state->arg_num == 0)
			arguments->input = arg;
		else if (state->arg_num == 1)
			arguments->output = arg;
		else if (state->arg_num == 2)
		{
			arguments->extra_imports = &state->argv[state->next-1];
			arguments->extra_imports_count = state->argc-state->next+1;
			state->next = state->argc;
		}
		else
			return ARGP_ERR_UNKNOWN;

		break;
	}

	case ARGP_KEY_END: {
		if (state->arg_num < 2)
			argp_usage(state);

		break;
	}

	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

void parse_arguments(int argc, char *argv[], elf_create_args *arguments)
{
	argp_parse(&argp, argc, argv, 0, 0, arguments);
}
