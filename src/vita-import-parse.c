#include <string.h>
#include <jansson.h>
#include "vita-import.h"

vita_imports_t *vita_imports_load(const char *filename, int verbose)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: could not open %s\n", filename);
		return NULL;
	}
	vita_imports_t *imports = vita_imports_loads(fp, verbose);

	fclose(fp);

	return imports;
}

vita_imports_t *vita_imports_loads(FILE *text, int verbose)
{
	json_t *libs, *lib_data;
	json_error_t error;
	const char *lib_name, *mod_name, *target_name;

	libs = json_loadf(text, 0, &error);
	if (libs == NULL) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return NULL;
	}

	if (!json_is_object(libs)) {
		fprintf(stderr, "error: modules is not an object\n");
		json_decref(libs);
		return NULL;
	}

	vita_imports_t *imports = vita_imports_new(json_object_size(libs));
	int i, j, k;

	i = -1;
	json_object_foreach(libs, lib_name, lib_data) {
		json_t *nid, *modules, *mod_data;

		i++;

		if (!json_is_object(lib_data)) {
			fprintf(stderr, "error: library %s is not an object\n", lib_name);
			json_decref(libs);
			return NULL;
		}

		nid = json_object_get(lib_data, "nid");
		if (!json_is_string(nid)) {
			fprintf(stderr, "error: library %s: nid is not a string\n", lib_name);
			json_decref(libs);
			return NULL;
		}

		modules = json_object_get(lib_data, "libraries");
		if (!json_is_object(modules)) {
			fprintf(stderr, "error: library %s: module is not an object\n", lib_name);
			json_decref(libs);
			return NULL;
		}

		imports->libs[i] = vita_imports_lib_new(
				lib_name,
				(int)strtol(json_string_value(nid), NULL, 0),
				json_object_size(modules));

		if (verbose)
			printf("Lib: %s\n", lib_name);

		j = -1;
		json_object_foreach(modules, mod_name, mod_data) {
			json_t *nid, *kernel, *functions, *variables, *target_nid;
			int has_variables = 1;

			j++;

			if (!json_is_object(mod_data)) {
				fprintf(stderr, "error: module %s is not an object\n", mod_name);
				json_decref(libs);
				return NULL;
			}

			nid = json_object_get(mod_data, "nid");
			if (!json_is_string(nid)) {
				fprintf(stderr, "error: module %s: nid is not a string\n", mod_name);
				json_decref(libs);
				return NULL;
			}

			kernel = json_object_get(mod_data, "kernel");
			if (!json_is_boolean(kernel)) {
				fprintf(stderr, "error: module %s: kernel is not a boolean\n", mod_name);
				json_decref(libs);
				return NULL;
			}

			functions = json_object_get(mod_data, "functions");
			if (!json_is_object(functions)) {
				fprintf(stderr, "error: module %s: functions is not an array\n", mod_name);
				json_decref(libs);
				return NULL;
			}

			variables = json_object_get(mod_data, "variables");
			if (variables == NULL) {
				has_variables = 0;
			}

			if (has_variables && !json_is_object(variables)) {
				fprintf(stderr, "error: module %s: variables is not an array\n", mod_name);
				json_decref(libs);
				return NULL;
			}

			if (verbose)
				printf("\tModule: %s\n", mod_name);

			imports->libs[i]->modules[j] = vita_imports_module_new(
					mod_name,
					json_boolean_value(kernel),
					(int)strtol(json_string_value(nid), NULL, 0),
					json_object_size(functions),
					json_object_size(variables));

			k = -1;
			json_object_foreach(functions, target_name, target_nid) {
				k++;

				if (!json_is_string(target_nid)) {
					fprintf(stderr, "error: function %s: nid is not a string\n", target_name);
					json_decref(libs);
					return NULL;
				}

				if (verbose)
					printf("\t\tFunction: %s\n", target_name);

				imports->libs[i]->modules[j]->functions[k] = vita_imports_stub_new(
						target_name,
						(int)strtol(json_string_value(target_nid), NULL, 0));
			}

			if (!has_variables) {
				continue;
			}

			k = -1;
			json_object_foreach(variables, target_name, target_nid) {
				k++;

				if (!json_is_string(target_nid)) {
					fprintf(stderr, "error: variable %s: nid is not a string\n", target_name);
					json_decref(libs);
					return NULL;
				}

				if (verbose)
					printf("\t\tVariable: %s\n", target_name);

				imports->libs[i]->modules[j]->variables[k] = vita_imports_stub_new(
						target_name,
						(int)strtol(json_string_value(target_nid), NULL, 0));

			}

		}
	}

	return imports;
}
