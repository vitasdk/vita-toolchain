
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "vita-nid-db.h"
#include "fs_list.h"
#include "yamltreeutil.h"


void vita_nid_db_create_entry(VitaNIDLibrary *library, const char *name, VitaNIDEntry **entry){

	VitaNIDEntry *current;

	*entry = NULL;

	current = malloc(sizeof(*current));

	current->next = NULL;
	current->prev = NULL;
	current->name = strdup(name);
	current->nid = 0xDEADBEEF;
	current->db = library->db;
	current->module = library->module;
	current->library = library;

	*entry = current;
}

void vita_nid_db_search_library_by_name(VitaNIDModule *module, const char *name, VitaNIDLibrary **library){

	VitaNIDLibrary *current;

	*library = NULL;

	current = module->Library.next;

	while(current != (VitaNIDLibrary *)&(module->Library)){
		if(module->firmware == current->firmware && strcmp(current->name, name) == 0){
			*library = current;
			break;
		}
		current = current->next;
	}
}

void vita_nid_db_search_or_create_library_by_name(VitaNIDModule *module, const char *name, VitaNIDLibrary **library){

	VitaNIDLibrary *current, *tail;

	vita_nid_db_search_library_by_name(module, name, library);

	if((*library) == NULL){
		current = malloc(sizeof(*current));
		current->next = NULL;
		current->prev = NULL;
		current->name = strdup(name);
		current->stubname = NULL;
		current->nid = 0xDEADBEEF;
		current->version = 0;
		current->firmware = module->firmware;
		current->is_kernel = 0;
		current->Function.next = (VitaNIDEntry *)&(current->Function);
		current->Function.prev = (VitaNIDEntry *)&(current->Function);
		current->Variable.next = (VitaNIDEntry *)&(current->Variable);
		current->Variable.prev = (VitaNIDEntry *)&(current->Variable);
		current->db = module->db;
		current->module = module;

		tail = module->Library.prev;

		current->next = tail->next;
		current->prev = tail;

		tail->next->prev = current;
		tail->next = current;

		*library = current;
	}
}

void vita_nid_db_search_module_by_name(VitaNIDDB *db, const char *name, VitaNIDModule **module){

	VitaNIDModule *current;

	*module = NULL;

	current = db->Module.next;

	while(current != (VitaNIDModule *)&(db->Module)){
		if(db->firmware == current->firmware && strcmp(current->name, name) == 0){
			*module = current;
			break;
		}
		current = current->next;
	}
}

void vita_nid_db_search_or_create_module_by_name(VitaNIDDB *db, const char *name, VitaNIDModule **module){

	VitaNIDModule *current, *tail;

	vita_nid_db_search_module_by_name(db, name, module);

	if((*module) == NULL){
		current = malloc(sizeof(*current));
		current->next = NULL;
		current->prev = NULL;
		current->name = strdup(name);
		current->fingerprint = 0xDEADBEEF;
		current->firmware = db->firmware;
		current->Library.next = (VitaNIDLibrary *)&(current->Library);
		current->Library.prev = (VitaNIDLibrary *)&(current->Library);
		current->db = db;

		tail = db->Module.prev;

		current->next = tail->next;
		current->prev = tail;

		tail->next->prev = current;
		tail->next = current;

		*module = current;
	}
}

void vita_nid_db_search_stub_by_name(VitaNIDDB *db, const char *name, uint32_t firmware, VitaNIDStub **stub){

	VitaNIDStub *current;

	*stub = NULL;

	current = db->Stub.next;

	while(current != (VitaNIDStub *)&(db->Stub)){
		if(firmware == current->firmware && strcmp(current->name, name) == 0){
			*stub = current;
			break;
		}
		current = current->next;
	}
}

void vita_nid_db_search_or_create_stub_by_name(VitaNIDDB *db, const char *name, uint32_t firmware, VitaNIDStub **stub){

	VitaNIDStub *current, *tail;

	vita_nid_db_search_stub_by_name(db, name, firmware, stub);

	if((*stub) == NULL){
		current = malloc(sizeof(*current));
		current->next = NULL;
		current->prev = NULL;
		current->firmware = firmware;

		char libname[0x100 + 0x10];

		if(firmware != 0 && (firmware & ~0xFFF) != (0x3600011 & ~0xFFF)){
			if((firmware & 0xFF000000) == 0){ // 0.990/0.995
				snprintf(libname, sizeof(libname), "%s_0%03X", name, firmware >> 12);
			}else if(((firmware >> 12) & 0xF) != 0){ // 1.692
				snprintf(libname, sizeof(libname), "%s_%03X", name, firmware >> 12);
			}else{
				snprintf(libname, sizeof(libname), "%s_%03X", name, firmware >> 16);
			}
		}else{
			snprintf(libname, sizeof(libname), "%s", name);
		}

		current->name = strdup(libname);
		current->LibStub.next = (VitaNIDLibStub *)&(current->LibStub);
		current->LibStub.prev = (VitaNIDLibStub *)&(current->LibStub);
		current->db = db;

		tail = db->Stub.prev;

		current->next = tail->next;
		current->prev = tail;

		tail->next->prev = current;
		tail->next = current;

		*stub = current;
	}
}

void vita_nid_db_library_push_entry_function(VitaNIDLibrary *Library, VitaNIDEntry *Entry){

	VitaNIDEntry *tail = Library->Function.prev;

	Entry->next = tail->next;
	Entry->prev = tail;

	tail->next->prev = Entry;
	tail->next = Entry;
}

void vita_nid_db_library_push_entry_variable(VitaNIDLibrary *Library, VitaNIDEntry *Entry){

	VitaNIDEntry *tail = Library->Variable.prev;

	Entry->next = tail->next;
	Entry->prev = tail;

	tail->next->prev = Entry;
	tail->next = Entry;
}

void vita_nid_db_stub_push_library(VitaNIDStub *stub, VitaNIDLibrary *library){

	VitaNIDLibStub *tail = stub->LibStub.prev;

	VitaNIDLibStub *libstub;

	libstub = malloc(sizeof(*libstub));

	libstub->next = tail->next;
	libstub->prev = tail;
	libstub->library = library;

	tail->next->prev = libstub;
	tail->next = libstub;
}

void vita_nid_db_entry_destroy(VitaNIDEntry *entry){
	free(entry->name);
	free(entry);
}

void vita_nid_db_library_destroy(VitaNIDLibrary *library){

	VitaNIDEntry *entry = library->Function.next;
	while(entry != (VitaNIDEntry *)&(library->Function)){
		VitaNIDEntry *next = entry->next;
		vita_nid_db_entry_destroy(entry);
		entry = next;
	}

	entry = library->Variable.next;
	while(entry != (VitaNIDEntry *)&(library->Variable)){
		VitaNIDEntry *next = entry->next;
		vita_nid_db_entry_destroy(entry);
		entry = next;
	}

	free(library->name);
	free(library->stubname);
	free(library);
}

void vita_nid_db_module_destroy(VitaNIDModule *module){

	VitaNIDLibrary *library = module->Library.next;
	while(library != (VitaNIDLibrary *)&(module->Library)){
		VitaNIDLibrary *next = library->next;
		vita_nid_db_library_destroy(library);
		library = next;
	}

	free(module->name);
	free(module);
}

void vita_nid_db_stub_destroy(VitaNIDStub *stub){

	VitaNIDLibStub *libstub = stub->LibStub.next;
	while(libstub != (VitaNIDLibStub *)&(stub->LibStub)){
		VitaNIDLibStub *next = libstub->next;
		free(libstub);
		libstub = next;
	}

	free(stub->name);
	free(stub);
}

void vita_nid_db_init(VitaNIDDB **_db){

	VitaNIDDB *db;

	db = malloc(sizeof(*db));
	db->firmware     = 0;
	db->Module.next  = (VitaNIDModule *)&(db->Module);
	db->Module.prev  = (VitaNIDModule *)&(db->Module);
	db->Stub.next    = (VitaNIDStub *)&(db->Stub);
	db->Stub.prev    = (VitaNIDStub *)&(db->Stub);
	*_db = db;
}

void vita_nid_db_destroy(VitaNIDDB *db){

	VitaNIDModule *module = db->Module.next;
	while(module != (VitaNIDModule *)&(db->Module)){
		VitaNIDModule *next = module->next;
		vita_nid_db_module_destroy(module);
		module = next;
	}

	VitaNIDStub *stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){
		VitaNIDStub *next = stub->next;
		vita_nid_db_stub_destroy(stub);
		stub = next;
	}

	free(db);
}


void vita_nid_db_print_stub(VitaNIDDB *db){


	VitaNIDStub *stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){

		printf("[%-31s]: Version=0x%08X\n", stub->name, stub->firmware);

		VitaNIDLibStub *libstub = stub->LibStub.next;
		while(libstub != (VitaNIDLibStub *)&(stub->LibStub)){

			printf("\t[%-31s]: NID=0x%08X\n", libstub->library->name, libstub->library->nid);

			libstub = libstub->next;
		}

		printf("\n");

		stub = stub->next;
	}
}



int is_number_ch(int ch){
	if((unsigned)(ch - '0') <= 9){
		return 1;
	}

	return 0;
}

int parse_fw_string(const char *s, uint32_t *v){

	if(strlen(s) > strlen("00.000.000")){
		printf("error %d\n", __LINE__);
		return -1;
	}

	if(s[0] == '.'){
		printf("error %d\n", __LINE__);
		return -1;
	}

	char fws[0x10];

	if(s[1] == '.'){
		snprintf(fws, sizeof(fws), "0%s", s);
	}else{
		snprintf(fws, sizeof(fws), "%s", s);
	}

	if(strlen(fws) == strlen("00.00")){
		snprintf(&(fws[5]), 8, "0.000");
	}else if(strlen(fws) == strlen("00.000")){
		snprintf(&(fws[6]), 8, ".000");
	}else if(strlen(fws) == strlen("00.000.000")){
	}

	if(fws[2] != '.'){return -1;}
	if(fws[6] != '.'){return -1;}

	// .
	fws[2] = fws[3];
	fws[3] = fws[4];
	fws[4] = fws[5];
	// .
	fws[5] = fws[7];
	fws[6] = fws[8];
	fws[7] = fws[9];
	fws[8] = 0;

	if(is_number_ch(fws[0]) == 0){return -1;}
	if(is_number_ch(fws[1]) == 0){return -1;}
	if(is_number_ch(fws[2]) == 0){return -1;}
	if(is_number_ch(fws[3]) == 0){return -1;}
	if(is_number_ch(fws[4]) == 0){return -1;}
	if(is_number_ch(fws[5]) == 0){return -1;}
	if(is_number_ch(fws[6]) == 0){return -1;}
	if(is_number_ch(fws[7]) == 0){return -1;}

	*v = strtol(fws, NULL, 16);

	return 0;
}


int process_entry(yaml_node *parent, yaml_node *child, VitaNIDLibrary *Library, void (* callback)(VitaNIDLibrary *Library, VitaNIDEntry *Entry)){

	VitaNIDEntry *entry;

	const char *left = parent->data.scalar.value;
	vita_nid_db_create_entry(Library, left, &entry);

	if(process_32bit_integer(child, &(entry->nid)) < 0){
		return -1;
	}

	callback(Library, entry);

	return 0;
}

int process_function_entry(yaml_node *parent, yaml_node *child, VitaNIDLibrary *Library){
	return process_entry(parent, child, Library, vita_nid_db_library_push_entry_function);
}

int process_variable_entry(yaml_node *parent, yaml_node *child, VitaNIDLibrary *Library){
	return process_entry(parent, child, Library, vita_nid_db_library_push_entry_variable);
}

int process_library_info(yaml_node *parent, yaml_node *child, VitaNIDLibrary *Library){

	if(is_scalar(parent) == 0){
		return -1;
	}

	int res;
	const char *left = parent->data.scalar.value;

	if(strcmp(left, "kernel") == 0){
		const char *right = child->data.scalar.value;
		if(strcmp(right, "false") == 0){
			Library->is_kernel = 0;
		}else if(strcmp(right, "true") == 0){
			Library->is_kernel = 1;
		}else{
			return -1;
		}

	}else if(strcmp(left, "nid") == 0){

		if(process_32bit_integer(child, &(Library->nid)) < 0){
			return -1;
		}

	}else if(strcmp(left, "version") == 0){

		if(process_32bit_integer(child, &(Library->version)) < 0){
			return -1;
		}

		if(Library->version >= 0x10000){
			return -1;
		}

	}else if(strcmp(left, "stubname") == 0){
		const char *right = child->data.scalar.value;
		Library->stubname = strdup(right);

	}else if(strcmp(left, "functions") == 0){
		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_function_entry, Library);
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
			res = yaml_iterate_mapping(child, (mapping_functor)process_variable_entry, Library);
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

int process_library_name(yaml_node *parent, yaml_node *child, VitaNIDModule *Module){

	if(is_scalar(parent) == 0){
		return -1;
	}

	const char *left = parent->data.scalar.value;

	VitaNIDLibrary *library;

	vita_nid_db_search_or_create_library_by_name(Module, left, &library);

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_library_info, library) < 0){
		return -1;
	}

	return 0;
}

int process_module_info(yaml_node *parent, yaml_node *child, VitaNIDModule *Module){

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
			res = yaml_iterate_mapping(child, (mapping_functor)process_library_name, Module);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}else if(strcmp(left, "nid") == 0 || strcmp(left, "fingerprint") == 0){
		if(process_32bit_integer(child, &(Module->fingerprint)) < 0){
			return -1;
		}
	}

	return 0;
}

int process_module_name(yaml_node *parent, yaml_node *child, VitaNIDDB *context){

	if(is_scalar(parent) == 0){
		return -1;
	}

	const char *left = parent->data.scalar.value;

	VitaNIDModule *module;

	vita_nid_db_search_or_create_module_by_name(context, left, &module);

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_info, module) < 0){
		return -1;
	}

	return 0;
}

int process_db_top(yaml_node *parent, yaml_node *child, VitaNIDDB *db){

	if(is_scalar(parent) == 0){
		return -1;
	}

	const char *left = parent->data.scalar.value;
	if(strcmp(left, "modules") == 0){
		if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_name, db) < 0){
			return -1;
		}
	}else if(strcmp(left, "firmware") == 0){
		parse_fw_string(child->data.scalar.value, &(db->firmware));
	}else if(strcmp(left, "version") == 0){
	}else{
		// printf("%s: %s\n", left, child->data.scalar.value);
	}

	return 0;
}

int parse_nid_db_yaml(yaml_document *doc, VitaNIDDB *db){

	if(is_mapping(doc) == 0){
		printf("error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		return -1;
	}

	return yaml_iterate_mapping(doc, (mapping_functor)process_db_top, db);
}

int add_nid_db_by_fp(VitaNIDDB *db, FILE *fp){

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

	db->firmware = 0;

	res = parse_nid_db_yaml(tree->docs[0], db);

	free_yaml_tree(tree);

	if(res < 0){
		return res;
	}

	return 0;
}

int db_top_list_callback(FSListEntry *ent, void *argp){

	int res;

	if(ent->isDir != 0){
		return 0;
	}

	FILE *fp = fopen(ent->path_full, "r");
	if(fp == NULL){
		return -1;
	}

	res = add_nid_db_by_fp(argp, fp);

	fclose(fp);
	fp = NULL;

	if(res < 0){
		printf("yml parse error on %s\n", ent->path_full);
	}

	return 0;
}


int vita_nid_db_mkdir(const char *name){
#if defined(_WIN32) && !defined(__CYGWIN__)
	return mkdir(name);
#else
	return mkdir(name, 0777); // create directory if it doesn't exist
#endif
}

void vita_nid_db_gen_asm(const char *dir, VitaNIDStub *stub, VitaNIDEntry *entry, int is_function){

	char path[0x400];

	snprintf(path, sizeof(path), "%s/%s", dir, stub->name);
	// vita_nid_db_mkdir(path);

	snprintf(path, sizeof(path), "%s/%s/%s", dir, stub->name, entry->library->name);
	// vita_nid_db_mkdir(path);

	snprintf(path, sizeof(path), "%s/%s/%s/%s.S", dir, stub->name, entry->library->name, entry->name);
	snprintf(path, sizeof(path), "%s/%s_%s_%s.S", dir, stub->name, entry->library->name, entry->name);

	FILE *fp = fopen(path, "wb");

	fprintf(fp, ".arch armv7a\n\n");

	if(is_function != 0){
		fprintf(fp, ".section .vitalink.fstubs.%s,\"ax\",%%progbits\n\n", entry->library->name);
	}else{
		fprintf(fp, ".section .vitalink.vstubs.%s,\"\",%%progbits\n\n", entry->library->name);
	}

	fprintf(fp, "\t.align 4\n");
	fprintf(fp, "\t.global %s\n", entry->name);

	if(is_function != 0){
		fprintf(fp, ".type %s, %%function\n", entry->name);
	}else{
		fprintf(fp, ".type %s, %%object\n", entry->name);
	}

	fprintf(fp, "%s:\n", entry->name);
	fprintf(fp, ".if GEN_WEAK_EXPORTS\n");
	fprintf(fp, "\t.word 0x%04X0008\n", entry->library->version);
	fprintf(fp, ".else\n");
	fprintf(fp, "\t.word 0x%04X0000\n", entry->library->version);
	fprintf(fp, ".endif //GEN_WEAK_EXPORTS\n");
	fprintf(fp, "\t.word 0x%08X\n", entry->library->nid);
	fprintf(fp, "\t.word 0x%08X\n", entry->nid);
	fprintf(fp, "\t.align 4\n");

	fclose(fp);
}

int gen_cmake(const char *dstdir, VitaNIDDB *db){

	vita_nid_db_mkdir(dstdir);

	char path[0x400];
	snprintf(path, sizeof(path), "%s/CMakeLists.txt", dstdir);

	FILE *fp = fopen(path, "wb");
	fprintf(fp,
		"cmake_minimum_required(VERSION 2.8)\n"
		"if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)\n"
		"\tif(DEFINED ENV{VITASDK})\n"
		"\t\tset(CMAKE_TOOLCHAIN_FILE \"$ENV{VITASDK}/share/vita.toolchain.cmake\" CACHE PATH \"toolchain file\")\n"
		"\telse()\n"
		"\t\tmessage(FATAL_ERROR \"Please define VITASDK to point to your SDK path!\")\n"
		"\tendif()\n"
		"endif()\n"
		"project(vitalibs)\n"
		"enable_language(ASM)\n"
		"\n"
	);

	VitaNIDStub *stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){

		fprintf(fp,
			"set(%s_ASM\n",
			stub->name
		);

		VitaNIDLibStub *libstub = stub->LibStub.next;
		while(libstub != (VitaNIDLibStub *)&(stub->LibStub)){

			VitaNIDLibrary *library = libstub->library;

			VitaNIDEntry *entry = library->Function.next;
			while(entry != (VitaNIDEntry *)&(library->Function)){
				vita_nid_db_gen_asm(dstdir, stub, entry, 1);
				// fprintf(fp, "\t%s/%s/%s.S\n", stub->name, entry->library->name, entry->name);
				fprintf(fp, "\t%s_%s_%s.S\n", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			entry = library->Variable.next;
			while(entry != (VitaNIDEntry *)&(library->Variable)){
				vita_nid_db_gen_asm(dstdir, stub, entry, 0);
				// fprintf(fp, "\t%s/%s/%s.S\n", stub->name, entry->library->name, entry->name);
				fprintf(fp, "\t%s_%s_%s.S\n", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			libstub = libstub->next;
		}

		fprintf(fp, ")\n\n");

		stub = stub->next;
	}

	fprintf(fp, "set(USER_LIBRARIES\n");

	stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){

		fprintf(fp,
			"\t\"%s\"\n",
			stub->name
		);

		stub = stub->next;
	}

	fprintf(fp, ")\n\n");

	fprintf(fp,
		"foreach(library ${USER_LIBRARIES})\n"
		"\tadd_library(${library}_stub STATIC ${${library}_ASM})\n"
		"\ttarget_compile_definitions(${library}_stub PRIVATE -DGEN_WEAK_EXPORTS=0)\n"
		"\tadd_library(${library}_stub_weak STATIC ${${library}_ASM})\n"
		"\ttarget_compile_definitions(${library}_stub_weak PRIVATE -DGEN_WEAK_EXPORTS=1)\n"
		"endforeach(library)\n"
	);

	fclose(fp);

	return 0;
}

int gen_makefile(const char *dstdir, VitaNIDDB *db){

	vita_nid_db_mkdir(dstdir);

	char path[0x400];
	snprintf(path, sizeof(path), "%s/makefile", dstdir);

	FILE *fp = fopen(path, "wb");
	fprintf(fp,
		"ifdef VITASDK\n"
		"PREFIX = $(VITASDK)/bin/\n"
		"endif\n\n"
		"ARCH ?= $(PREFIX)arm-vita-eabi\n"
		"AS = $(ARCH)-as\n"
		"AR = $(ARCH)-ar\n"
		"RANLIB = $(ARCH)-ranlib\n\n"
	);

	fprintf(fp, "TARGETS =");

	VitaNIDStub *stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){
		fprintf(fp, " lib%s_stub.a", stub->name);
		stub = stub->next;
	}

	fprintf(fp, "\n");
	fprintf(fp, "TARGETS_WEAK =");

	stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){
		fprintf(fp, " lib%s_stub_weak.a", stub->name);
		stub = stub->next;
	}

	fprintf(fp, "\n");

	stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){

		fprintf(fp, "%s_OBJS =", stub->name);

		VitaNIDLibStub *libstub = stub->LibStub.next;
		while(libstub != (VitaNIDLibStub *)&(stub->LibStub)){

			VitaNIDLibrary *library = libstub->library;

			VitaNIDEntry *entry = library->Function.next;
			while(entry != (VitaNIDEntry *)&(library->Function)){
				vita_nid_db_gen_asm(dstdir, stub, entry, 1);
				// fprintf(fp, " %s/%s/%s.o", stub->name, entry->library->name, entry->name);
				fprintf(fp, " %s_%s_%s.o", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			entry = library->Variable.next;
			while(entry != (VitaNIDEntry *)&(library->Variable)){
				vita_nid_db_gen_asm(dstdir, stub, entry, 0);
				// fprintf(fp, " %s/%s/%s.o", stub->name, entry->library->name, entry->name);
				fprintf(fp, " %s_%s_%s.o", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			libstub = libstub->next;
		}

		fprintf(fp, "\n");

		stub = stub->next;
	}

	stub = db->Stub.next;
	while(stub != (VitaNIDStub *)&(db->Stub)){

		fprintf(fp, "%s_weak_OBJS =", stub->name);

		VitaNIDLibStub *libstub = stub->LibStub.next;
		while(libstub != (VitaNIDLibStub *)&(stub->LibStub)){

			VitaNIDLibrary *library = libstub->library;

			VitaNIDEntry *entry = library->Function.next;
			while(entry != (VitaNIDEntry *)&(library->Function)){
				// fprintf(fp, " %s/%s/%s.wo", stub->name, entry->library->name, entry->name);
				fprintf(fp, " %s_%s_%s.wo", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			entry = library->Variable.next;
			while(entry != (VitaNIDEntry *)&(library->Variable)){
				// fprintf(fp, " %s/%s/%s.wo", stub->name, entry->library->name, entry->name);
				fprintf(fp, " %s_%s_%s.wo", stub->name, entry->library->name, entry->name);
				entry = entry->next;
			}

			libstub = libstub->next;
		}

		fprintf(fp, "\n");

		stub = stub->next;
	}

	fprintf(fp, "SceKernel_OBJS =\n");
	fprintf(fp, "ALL_OBJS=\n");
	fprintf(fp, "\n");
	fprintf(fp, "all: $(TARGETS) $(TARGETS_WEAK)\n\n");
	fprintf(fp, "define LIBRARY_template\n");
	fprintf(fp, " $(1): $$($(1:lib%%_stub.a=%%)_OBJS)\n");
	fprintf(fp, " ALL_OBJS += $$($(1:lib%%_stub.a=%%)_OBJS)\n");
	fprintf(fp, "endef\n");
	fprintf(fp, "define LIBRARY_WEAK_template\n");
	fprintf(fp, " $(1): $$($(1:lib%%_stub_weak.a=%%)_weak_OBJS)\n");
	fprintf(fp, " ALL_OBJS += $$($(1:lib%%_stub_weak.a=%%)_weak_OBJS)\n");
	fprintf(fp, "endef\n\n");
	fprintf(fp, "$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))\n");
	fprintf(fp, "$(foreach library,$(TARGETS_WEAK),$(eval $(call LIBRARY_WEAK_template,$(library))))\n\n");
	fprintf(fp, "install: $(TARGETS) $(TARGETS_WEAK)\n");
	fprintf(fp, "\tcp $(TARGETS) $(VITASDK)/arm-vita-eabi/lib\n");
	fprintf(fp, "\tcp $(TARGETS_WEAK) $(VITASDK)/arm-vita-eabi/lib\n\n");
	fprintf(fp, "clean:\n");
	fprintf(fp, "\trm -f $(TARGETS) $(TARGETS_WEAK) $(ALL_OBJS)\n\n");
	fprintf(fp, "$(TARGETS) $(TARGETS_WEAK):\n");
	fprintf(fp, "\t@echo \"$?\" > $@-objs\n");
	fprintf(fp, "\t$(AR) cru $@ @$@-objs\n");
	fprintf(fp, "\t$(RANLIB) $@\n");
	fprintf(fp, "\trm $^ $@-objs\n\n");
	fprintf(fp, "%%.o: %%.S\n");
	fprintf(fp, "\t$(AS) --defsym GEN_WEAK_EXPORTS=0 $< -o $@\n\n");
	fprintf(fp, "%%.wo: %%.S\n");
	fprintf(fp, "\t$(AS) --defsym GEN_WEAK_EXPORTS=1 $< -o $@\n");
	fprintf(fp, "\trm $^\n\n");

	fclose(fp);

	return 0;
}

const char *find_item(int argc, char *argv[], const char *name){

	for(int i=0;i<argc;i++){
		if(strstr(argv[i], name) != NULL){
			return (char *)(strstr(argv[i], name) + strlen(name));
		}
	}

	return NULL;
}

int main(int argc, char *argv[]){

	VitaNIDDB *db;

	const char *cmake = find_item(argc, argv, "-cmake=");
	const char *yml = find_item(argc, argv, "-yml=");
	const char *output = find_item(argc, argv, "-output=");

	vita_nid_db_init(&db);

	int res;
	struct stat stat_buf;

	res = stat(yml, &stat_buf);
	if(res != 0){
		return EXIT_FAILURE;
	}

	if(S_ISDIR(stat_buf.st_mode)){

		FSListEntry *nid_db_list = NULL;

		res = fs_list_init(&nid_db_list, yml, NULL, NULL);
		if(res >= 0){
			res = fs_list_execute(nid_db_list->child, db_top_list_callback, db);
		}
		fs_list_fini(nid_db_list);
		nid_db_list = NULL;

	}else if(S_ISREG(stat_buf.st_mode)){

		FILE *fp = fopen(yml, "r");
		if(fp == NULL){
			return EXIT_FAILURE;
		}

		res = add_nid_db_by_fp(db, fp);

		fclose(fp);
		fp = NULL;
	}

	VitaNIDModule *module = db->Module.next;
	while(module != (VitaNIDModule *)&(db->Module)){

		VitaNIDLibrary *library = module->Library.next;
		while(library != (VitaNIDLibrary *)&(module->Library)){

			VitaNIDStub *stub;

			if(library->stubname != NULL){
				vita_nid_db_search_or_create_stub_by_name(db, library->stubname, library->firmware, &stub);
			}else if(library->is_kernel != 0){
				vita_nid_db_search_or_create_stub_by_name(db, library->name, library->firmware, &stub);
			}else{
				vita_nid_db_search_or_create_stub_by_name(db, library->module->name, library->firmware, &stub);
			}

			vita_nid_db_stub_push_library(stub, library);

			library = library->next;
		}
		module = module->next;
	}

	// gen_cmake(dstdir, db);
	gen_makefile(output, db);

	// vita_nid_db_print_stub(db);

	vita_nid_db_destroy(db);

	return EXIT_SUCCESS;
}
