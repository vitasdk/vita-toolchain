#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "vita-import.h"

vita_imports_t *vita_imports_load(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: could not open %s\n", filename);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *text = malloc(size * sizeof(char));

	fread(text, 1, size, fp);
	fclose(fp);

	vita_imports_t *imports = vita_imports_loads(text);

	free(text);
	return imports;
}

vita_imports_t *vita_imports_loads(const char *text)
{
	json_t *libs;
	json_error_t error;

	libs = json_loads(text, 0, &error);
	if (libs == NULL) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return NULL;
	}

	if (!json_is_array(libs)) {
		fprintf(stderr, "error: modules is not an array\n");
		json_decref(libs);
		return NULL;
	}

	vita_imports_t *imports = vita_imports_new(json_array_size(libs));

	int i, j, k;
	for (i = 0; i < json_array_size(libs); i++) {
		json_t *data, *name, *nid, *modules;

		data = json_array_get(libs, i);

		if (!json_is_object(data)) {
			fprintf(stderr, "error: %d is not an object\n", i + 1);
			json_decref(libs);
			return NULL;
		}

		name = json_object_get(data, "name");
		if (!json_is_string(name)) {
			fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
			json_decref(libs);
			return NULL;
		}

		nid = json_object_get(data, "nid");
		if (!json_is_integer(nid)) {
			fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
			json_decref(libs);
			return NULL;
		}

		modules = json_object_get(data, "modules");
		if (!json_is_array(modules)) {
			fprintf(stderr, "error: name %d: module is not an array\n", i + 1);
			json_decref(libs);
			return NULL;
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
				return NULL;
			}

			name = json_object_get(data, "name");
			if (!json_is_string(name)) {
				fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
				json_decref(libs);
				return NULL;
			}

			nid = json_object_get(data, "nid");
			if (!json_is_integer(nid)) {
				fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
				json_decref(libs);
				return NULL;
			}

			kernel = json_object_get(data, "kernel");
			if (!json_is_boolean(kernel)) {
				fprintf(stderr, "error: name %d: kernel is not a boolean\n", i + 1);
				json_decref(libs);
				return NULL;
			}

			functions = json_object_get(data, "functions");
			if (!json_is_array(functions)) {
				fprintf(stderr, "error: name %d: functions is not an array\n", i + 1);
				json_decref(libs);
				return NULL;
			}

			variables = json_object_get(data, "variables");
			if (!json_is_array(variables)) {
				fprintf(stderr, "error: name %d: variables is not an array\n", i + 1);
				json_decref(libs);
				return NULL;
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
					return NULL;
				}

				name = json_object_get(data, "name");
				if (!json_is_string(name)) {
					fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
					json_decref(libs);
					return NULL;
				}

				nid = json_object_get(data, "nid");
				if (!json_is_integer(nid)) {
					fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
					json_decref(libs);
					return NULL;
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
					return NULL;
				}

				name = json_object_get(data, "name");
				if (!json_is_string(name)) {
					fprintf(stderr, "error: name %d: name is not a string\n", i + 1);
					json_decref(libs);
					return NULL;
				}

				nid = json_object_get(data, "nid");
				if (!json_is_integer(nid)) {
					fprintf(stderr, "error: name %d: nid is not an integer\n", i + 1);
					json_decref(libs);
					return NULL;
				}

				printf("\t\tVariable: %s\n", json_string_value(name));

				imports->libs[i]->modules[j]->variables[k] = vita_imports_stub_new(
					json_string_value(name),
					json_integer_value(nid));

			}



		}
	}

	return imports;
}
