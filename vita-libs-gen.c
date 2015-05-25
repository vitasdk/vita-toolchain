#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "vita-import.h"

void usage();
int generate_assembly(vita_imports_t *imports);

int main(int argc, char *argv[])
{
	if (argc < 2) {
		usage();
		goto exit_failure;
	}

	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: could not open %s\n", argv[1]);
		goto exit_failure;
	}
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *text = malloc(size * sizeof(char));

	fread(text, 1, size, fp);
	fclose(fp);

	json_t *libs;
	json_error_t error;

	libs = json_loads(text, 0, &error);
	if (libs == NULL) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		free(text);
		goto exit_failure;
	}
	free(text);

	if (!json_is_array(libs)) {
		fprintf(stderr, "error: modules is not an array\n");
		json_decref(libs);
		return 1;
	}

	vita_imports_t *imports = vita_imports_new(json_array_size(libs));

	int i, j, k;
	for (i = 0; i < json_array_size(libs); i++) {
		json_t *data, *name, *nid, *modules;

		data = json_array_get(libs, i);

		if (!json_is_object(data)) {
			fprintf(stderr, "error: %d is not an object\n", i + 1);
			json_decref(libs);
			return 1;
		}

		name = json_object_get(data, "name");
		if (!json_is_string(name)) {
			fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
			json_decref(libs);
			return 1;
		}

		nid = json_object_get(data, "nid");
		if (!json_is_integer(nid)) {
			fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
			json_decref(libs);
			return 1;
		}

		modules = json_object_get(data, "modules");
		if (!json_is_array(modules)) {
			fprintf(stderr, "error: name %d: module is not an array\n", i + 1);
			json_decref(libs);
			return 1;
		}

		imports->libs[i] = vita_imports_lib_new(
			json_string_value(name),
			json_integer_value(nid),
			json_array_size(modules));

		printf("Lib: %s\n", json_string_value(name));

		for (j = 0; j < json_array_size(modules); j++) {
			json_t *data, *name, *nid, *kernel, *functions, *variables;

			data = json_array_get(modules, j);

			if (!json_is_object(data)) {
				fprintf(stderr, "error: %d is not an object\n", i + 1);
				json_decref(libs);
				return 1;
			}

			name = json_object_get(data, "name");
			if (!json_is_string(name)) {
				fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
				json_decref(libs);
				return 1;
			}

			nid = json_object_get(data, "nid");
			if (!json_is_integer(nid)) {
				fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
				json_decref(libs);
				return 1;
			}

			kernel = json_object_get(data, "kernel");
			if (!json_is_boolean(kernel)) {
				fprintf(stderr, "error: name %d: kernel is not a boolean\n", i + 1);
				json_decref(libs);
				return 1;
			}

			functions = json_object_get(data, "functions");
			if (!json_is_array(functions)) {
				fprintf(stderr, "error: name %d: functions is not an array\n", i + 1);
				json_decref(libs);
				return 1;
			}

			variables = json_object_get(data, "variables");
			if (!json_is_array(variables)) {
				fprintf(stderr, "error: name %d: variables is not an array\n", i + 1);
				json_decref(libs);
				return 1;
			}

			printf("\tModule: %s\n", json_string_value(name));

			imports->libs[i]->modules[j] = vita_imports_module_new(
				json_string_value(name),
				json_integer_value(nid),
				json_array_size(functions),
				json_array_size(variables));

			for (k = 0; k < json_array_size(functions); k++) {
				json_t *data, *name, *nid;

				data = json_array_get(functions, k);

				if (!json_is_object(data)) {
					fprintf(stderr, "error: %d is not an object\n", i + 1);
					json_decref(libs);
					return 1;
				}

				name = json_object_get(data, "name");
				if (!json_is_string(name)) {
					fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
					json_decref(libs);
					return 1;
				}

				nid = json_object_get(data, "nid");
				if (!json_is_integer(nid)) {
					fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
					json_decref(libs);
					return 1;
				}

				printf("\t\tFunction: %s\n", json_string_value(name));

				imports->libs[i]->modules[j]->functions[k] = vita_imports_stub_new(
					json_string_value(name),
					json_integer_value(nid));

			}

			for (k = 0; k < json_array_size(variables); k++) {
				json_t *data, *name, *nid;

				data = json_array_get(variables, k);

				if (!json_is_object(data)) {
					fprintf(stderr, "error: %d is not an object\n", i + 1);
					json_decref(libs);
					return 1;
				}

				name = json_object_get(data, "name");
				if (!json_is_string(name)) {
					fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
					json_decref(libs);
					return 1;
				}

				nid = json_object_get(data, "nid");
				if (!json_is_integer(nid)) {
					fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
					json_decref(libs);
					return 1;
				}

				printf("\t\tVariable: %s\n", json_string_value(name));

				imports->libs[i]->modules[j]->variables[k] = vita_imports_stub_new(
					json_string_value(name),
					json_integer_value(nid));

			}



		}
	}

	if (!generate_assembly(imports)) {
		fprintf(stderr, "Error generating the assembly file\n");
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

		for (j = 0; j < imports->libs[i]->n_modules; j++) {

			snprintf(filename, 128, "%s.S", imports->libs[i]->modules[j]->name);

			if ((fp = fopen(filename, "w")) == NULL) {
				return 0;
			}

			fprintf(fp, "@ %s\n", filename);
			fprintf(fp, "@ this file was automagically generated by vita-libs-gen by xerpi\n\n");
			fprintf(fp, "@ module: %s\n\n", imports->libs[i]->modules[j]->name);
			fprintf(fp, ".arch armv7−a\n\n");

			if (imports->libs[i]->modules[j]->n_functions > 0)
				fprintf(fp, ".section .vitalink.fstubs,\"ax\",%%progbits\n\n");

			for (k = 0; k < imports->libs[i]->modules[j]->n_functions; k++) {

				const char *fname = imports->libs[i]->modules[j]->functions[k]->name;
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%function\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n\n",
					fname, fname, fname,
					imports->libs[i]->NID,
					imports->libs[i]->modules[j]->NID,
					imports->libs[i]->modules[j]->functions[k]->NID);
			}
			fprintf(fp, "\n");

			if (imports->libs[i]->modules[j]->n_variables > 0)
				fprintf(fp, ".section .vitalink.vstubs,\"awx\",%%progbits\n\n");

			for (k = 0; k < imports->libs[i]->modules[j]->n_variables; k++) {

				const char *vname = imports->libs[i]->modules[j]->variables[k]->name;
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%object\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n\n",
					vname, vname, vname,
					imports->libs[i]->NID,
					imports->libs[i]->modules[j]->NID,
					imports->libs[i]->modules[j]->variables[k]->NID);
			}

			fprintf(fp, "@ end of the file\n");
			fclose(fp);
		}
	}

	return 1;
}

void usage()
{
	fprintf(stderr,
		"vita-libs-gen by xerpi\n"
		"usage:\n\tvita-libs-gen in.json\n"
	);
}
