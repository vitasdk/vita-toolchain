
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vita-nid-db-yml.h"
#include "utils/yamltreeutil.h"


const VitaNIDCallbacks *g_VitaNIDCallbacks = NULL;

int g_VitaNIDCallbacks_register(const VitaNIDCallbacks *pVitaNIDCallbacks){

	if(pVitaNIDCallbacks->size != sizeof(*pVitaNIDCallbacks)){
		return -1;
	}

	g_VitaNIDCallbacks = pVitaNIDCallbacks;

	return 0;
}

static int call_database_version(const char *version, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->database_version == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->database_version(version, argp);
}

static int call_database_firmware(const char *firmware, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->database_firmware == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->database_firmware(firmware, argp);
}

static int call_module_name(const char *name, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->module_name == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->module_name(name, argp);
}

static int call_module_fingerprint(uint32_t fingerprint, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->module_fingerprint == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->module_fingerprint(fingerprint, argp);
}

static int call_library_name(const char *name, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->library_name == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->library_name(name, argp);
}

static int call_library_stubname(const char *name, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->library_stubname == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->library_stubname(name, argp);
}

static int call_library_version(uint32_t version, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->library_version == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->library_version(version, argp);
}

static int call_library_nid(uint32_t nid, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->library_nid == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->library_nid(nid, argp);
}

static int call_library_privilege(const char *privilege, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->library_privilege == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->library_privilege(privilege, argp);
}

static int call_entry_function(const char *name, uint32_t nid, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->entry_function == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->entry_function(name, nid, argp);
}

static int call_entry_variable(const char *name, uint32_t nid, void *argp){

	if(g_VitaNIDCallbacks == NULL || g_VitaNIDCallbacks->entry_variable == NULL){
		return 0;
	}

	return g_VitaNIDCallbacks->entry_variable(name, nid, argp);
}

int process_entry(yaml_node *parent, yaml_node *child, void *argp, int (* callback)(const char *name, uint32_t nid, void *argp)){

	uint32_t nid = 0;

	if(process_32bit_integer(child, &nid) < 0){
		return -1;
	}

	return callback(parent->data.scalar.value, nid, argp);
}

int process_function_entry(yaml_node *parent, yaml_node *child, void *argp){
	return process_entry(parent, child, argp, call_entry_function);
}

int process_variable_entry(yaml_node *parent, yaml_node *child, void *argp){
	return process_entry(parent, child, argp, call_entry_variable);
}

int process_library_info(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	int res;
	const char *left = parent->data.scalar.value;

	if(strcmp(left, "kernel") == 0){
		const char *right = child->data.scalar.value;
		if(strcmp(right, "false") == 0){
			call_library_privilege("user", argp);
		}else if(strcmp(right, "true") == 0){
			call_library_privilege("kernel", argp);
		}else{
			return -1;
		}

	}else if(strcmp(left, "nid") == 0){

		uint32_t nid = 0;

		if(process_32bit_integer(child, &nid) < 0){
			return -1;
		}

		call_library_nid(nid, argp);

	}else if(strcmp(left, "version") == 0){

		uint32_t version = 0;

		if(process_32bit_integer(child, &version) < 0){
			return -1;
		}

		call_library_version(version, argp);

	}else if(strcmp(left, "stubname") == 0){

		call_library_stubname(child->data.scalar.value, argp);

	}else if(strcmp(left, "functions") == 0){
		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_function_entry, argp);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}else if(strcmp(left, "variables") == 0){
		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_variable_entry, argp);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}

	return 0;
}

int process_library_name(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	call_library_name(parent->data.scalar.value, argp);

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_library_info, argp) < 0){
		return -1;
	}

	return 0;
}

int process_module_info(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	int res;
	const char *left = parent->data.scalar.value;

	if(is_scalar(parent) == 0){
		return -1;
	}

	if(strcmp(left, "libraries") == 0){
		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_library_name, argp);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}else if(strcmp(left, "nid") == 0 || strcmp(left, "fingerprint") == 0){

		uint32_t fingerprint = 0xDEADBEEF;

		if(process_32bit_integer(child, &fingerprint) < 0){
			return -1;
		}

		call_module_fingerprint(fingerprint, argp);
	}

	return 0;
}

int process_module_name(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	call_module_name(parent->data.scalar.value, argp);

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_info, argp) < 0){
		return -1;
	}

	return 0;
}

int process_db_top(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	const char *left = parent->data.scalar.value;
	if(strcmp(left, "modules") == 0){
		if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_name, argp) < 0){
			return -1;
		}
	}else if(strcmp(left, "firmware") == 0){
		call_database_firmware(child->data.scalar.value, argp);
	}else if(strcmp(left, "version") == 0){
		call_database_version(child->data.scalar.value, argp);
	}

	return 0;
}

int parse_nid_db_yaml(yaml_document *doc, void *argp){

	if(is_mapping(doc) == 0){
		printf("error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		return -1;
	}

	return yaml_iterate_mapping(doc, (mapping_functor)process_db_top, argp);
}

int add_nid_db_by_fp(FILE *fp, void *argp){

	int res;
	yaml_error error;
	yaml_tree *tree;

	memset(&error, 0, sizeof(error));

	tree = parse_yaml_stream(fp, &error);
	if(tree == NULL){
		printf("error: %s\n", error.problem);
		free(error.problem);
		return -1;
	}

	res = parse_nid_db_yaml(tree->docs[0], argp);

	free_yaml_tree(tree);

	if(res < 0){
		return res;
	}

	return 0;
}

int add_nid_db_by_path(const char *path, void *argp){

	int res;

	FILE *fp = fopen(path, "r");
	if(fp == NULL){
		return -1;
	}

	res = add_nid_db_by_fp(fp, argp);

	fclose(fp);
	fp = NULL;

	if(res < 0){
		// printf("yml parse error on %s\n", path);
		return res;
	}

	return 0;
}
