
#include <stdlib.h>
#include <string.h>
#include "vita-nid-bypass.h"
#include "utils/yamltreeutil.h"

NIDDbBypassEntry *search_or_create_entry_by_name(NIDDbBypassLibrary *library, const char *name){

	NIDDbBypassEntry *entry = library->entry.next;

	while(entry != (NIDDbBypassEntry *)&(library->entry)){
		if(strcmp(name, entry->name) == 0){
			return entry;
		}

		entry = entry->next;
	}

	NIDDbBypassEntry *tail = library->entry.prev;

	entry = malloc(sizeof(*entry));
	entry->next = tail->next;
	entry->prev = tail;
	entry->name = strdup(name);

	tail->next->prev = entry;
	tail->next = entry;

	return entry;
}

NIDDbBypassLibrary *search_or_create_library(NIDDbBypass *bypass, const char *name){

	NIDDbBypassLibrary *library = bypass->library.next;

	while(library != (NIDDbBypassLibrary *)&(bypass->library)){
		if(strcmp(name, library->name) == 0){
			return library;
		}

		library = library->next;
	}

	NIDDbBypassLibrary *tail = bypass->library.prev;

	library = malloc(sizeof(*library));
	library->next = tail->next;
	library->prev = tail;
	library->name = strdup(name);
	library->entry.next = (NIDDbBypassEntry *)&(library->entry);
	library->entry.prev = (NIDDbBypassEntry *)&(library->entry);

	tail->next->prev = library;
	tail->next = library;

	return library;
}

NIDDbBypassEntry *nid_db_bypass_search_entry_by_name(NIDDbBypassLibrary *library, const char *name){

	NIDDbBypassEntry *entry = library->entry.next;

	while(entry != (NIDDbBypassEntry *)&(library->entry)){
		if(strcmp(entry->name, name) == 0){
			return entry;
		}

		entry = entry->next;
	}

	return NULL;
}

NIDDbBypassLibrary *nid_db_bypass_search_library(NIDDbBypass *bypass, const char *name){

	NIDDbBypassLibrary *library = bypass->library.next;

	while(library != (NIDDbBypassLibrary *)&(bypass->library)){
		if(strcmp(library->name, name) == 0){
			return library;
		}

		library = library->next;
	}

	return NULL;
}

int process_nid_db_bypass_entry(yaml_node *entry, void *argp){

	if(is_scalar(entry) == 0){
		return -1;
	}

	search_or_create_entry_by_name(argp, entry->data.scalar.value);

	return 0;
}

int process_nid_db_bypass_libraries(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	NIDDbBypass *bypass = argp;

	NIDDbBypassLibrary *library = search_or_create_library(bypass, parent->data.scalar.value);

	if(is_sequence(child) != 0 && yaml_iterate_sequence(child, process_nid_db_bypass_entry, library) < 0){
		return -1;
	}

	return 0;
}

int process_nid_db_bypass_root(yaml_node *parent, yaml_node *child, void *argp){

	if(is_scalar(parent) == 0){
		return -1;
	}

	const char *left = parent->data.scalar.value;
	if(strcmp(left, "bypass") == 0 && is_mapping(child) != 0){
		if(yaml_iterate_mapping(child, process_nid_db_bypass_libraries, argp) < 0){
			return -1;
		}
	}

	return 0;
}

int parse_nid_db_bypass_yaml(yaml_document *doc, void *argp){

	if(is_mapping(doc) == 0){
		printf("error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		return -1;
	}

	return yaml_iterate_mapping(doc, process_nid_db_bypass_root, argp);
}

int load_nid_db_bypass_by_fp(FILE *fp, void *argp){

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

	res = parse_nid_db_bypass_yaml(tree->docs[0], argp);

	free_yaml_tree(tree);

	if(res < 0){
		return res;
	}

	return 0;
}

int load_nid_db_bypass_by_path(const char *path, void *argp){

	int res;

	FILE *fp = fopen(path, "r");
	if(fp == NULL){
		return -1;
	}

	res = load_nid_db_bypass_by_fp(fp, argp);

	fclose(fp);
	fp = NULL;

	if(res < 0){
		printf("yml parse error on %s\n", path);
	}

	return 0;
}
