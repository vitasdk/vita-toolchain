#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "vita-import.h"

void usage();
int generate_assembly(vita_imports_t *imports, const char *filename);

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

	json_t *modules;
	json_error_t error;

	modules = json_loads(text, 0, &error);
	if (modules == NULL) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		free(text);
		goto exit_failure;
	}
	free(text);

	if (!json_is_array(modules)) {
		fprintf(stderr, "error: modules is not an array\n");
		json_decref(modules);
		return 1;
	}

	vita_imports_t *imports = vita_imports_new(json_array_size(modules));

	int i, j, k;
	for (i = 0; i < json_array_size(modules); i++) {
		json_t *data, *name, *nid, *libs;

		data = json_array_get(modules, i);

		if (!json_is_object(data)) {
			fprintf(stderr, "error: %d is not an object\n", i + 1);
			json_decref(modules);
			return 1;
		}

		name = json_object_get(data, "name");
		if (!json_is_string(name)) {
			fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
			json_decref(modules);
			return 1;
		}

		nid = json_object_get(data, "nid");
		if (!json_is_integer(nid)) {
			fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
			json_decref(modules);
			return 1;
		}

		libs = json_object_get(data, "modules");
		if (!json_is_array(libs)) {
			libs = json_object_get(data, "libs");
			if (!json_is_array(libs)) {
				fprintf(stderr, "error: name %d: module is not an array\n", i + 1);
				json_decref(libs);
				return 1;
			}
		}

		imports->modules[i] = vita_imports_module_new(
			json_string_value(name),
			json_integer_value(nid),
			json_array_size(libs));

		printf("Module: %s\n", json_string_value(name));

		for (j = 0; j < json_array_size(libs); j++) {
			json_t *data, *name, *nid, *kernel, *functions, *variables;

			data = json_array_get(libs, j);

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

			printf("\tLib: %s\n", json_string_value(name));

			imports->modules[i]->libs[j] = vita_imports_lib_new(
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

				imports->modules[i]->libs[j]->functions[k] = vita_imports_stub_new(
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

				imports->modules[i]->libs[j]->variables[k] = vita_imports_stub_new(
					json_string_value(name),
					json_integer_value(nid));

			}



		}
	}

	if (!generate_assembly(imports, (argc > 2) ? argv[2] : "out.S")) {
		fprintf(stderr, "Error generating the assembly file\n");
	}

	vita_imports_free(imports);

	return EXIT_SUCCESS;
exit_failure:
	return EXIT_FAILURE;
}


int generate_assembly(vita_imports_t *imports, const char *filename)
{
	int i, j, k;
	FILE *fp = fopen(filename, "w");
	if (fp == NULL)
		return 0;

	fprintf(fp, "@ this file was automagically generated by vita-libs-gen by xerpi\n");
	fprintf(fp, ".arch armv7−a\n\n");

	fprintf(fp, ".section .vitalink.fstubs,\"ax\",%%progbits\n");

	for (i = 0; i < imports->n_modules; i++) {

		fprintf(fp, "@ module: %s\n\n", imports->modules[i]->name);

		for (j = 0; j < imports->modules[i]->n_libs; j++) {

			fprintf(fp, "@ lib: %s\n\n", imports->modules[i]->libs[j]->name);

			for (k = 0; k < imports->modules[i]->libs[j]->n_functions; k++) {

				const char *fname = imports->modules[i]->libs[j]->functions[k]->name;
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%function\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n\n",
					fname, fname, fname,
					imports->modules[i]->NID,
					imports->modules[i]->libs[j]->NID,
					imports->modules[i]->libs[j]->functions[k]->NID);
			}
			fprintf(fp, "\n");
		}
		fprintf(fp, "\n");
	}

	fprintf(fp, ".section .vitalink.vstubs,\"awx\",%%progbits\n");

	for (i = 0; i < imports->n_modules; i++) {

		fprintf(fp, "@ module: %s\n\n", imports->modules[i]->name);

		for (j = 0; j < imports->modules[i]->n_libs; j++) {

			fprintf(fp, "@ lib: %s\n\n", imports->modules[i]->libs[j]->name);

			for (k = 0; k < imports->modules[i]->libs[j]->n_variables; k++) {

				const char *vname = imports->modules[i]->libs[j]->variables[k]->name;
				fprintf(fp,
					"\t.align 4\n"
					"\t.global %s\n"
					"\t.type %s, %%object\n"
					"%s:\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n"
					"\t.word 0x%08X\n\n",
					vname, vname, vname,
					imports->modules[i]->NID,
					imports->modules[i]->libs[j]->NID,
					imports->modules[i]->libs[j]->variables[k]->NID);
			}
			fprintf(fp, "\n");
		}
		fprintf(fp, "\n");
	}

	fprintf(fp, "@ end of the file\n");
	fclose(fp);

	return 1;
}

void usage()
{
	fprintf(stderr,
		"vita-libs-gen by xerpi\n"
		"usage:\n\tvita-libs-gen in.json [out.S]\n"
	);
}
