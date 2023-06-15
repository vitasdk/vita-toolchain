

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "fs_list.h"
#include "yamltreeutil.h"


#define VITASDK_NID_CHK_ERROR                (0x80010001)
#define VITASDK_NID_CHK_ERROR_NO_MEMORY      (0x80010002)
#define VITASDK_NID_CHK_ERROR_OPEN_FAILED    (0x80010003)
#define VITASDK_NID_CHK_ERROR_CHECK_FAILED   (0x80010004)
#define VITASDK_NID_CHK_ERROR_DUPLICATE_NAME (0x80010005)
#define VITASDK_NID_CHK_ERROR_DIFF_TYPE      (0x80010006)
#define VITASDK_NID_CHK_ERROR_INVALID_FORMAT (0x80010007)
#define VITASDK_NID_CHK_ERROR_BAD_SORT       (0x80010008)

#define ENTRY_TYPE_FUNCTION (0)
#define ENTRY_TYPE_VARUABLE (1)

#define LIBRARY_LOCATE_USERMODE (0)
#define LIBRARY_LOCATE_KERNEL   (1)

typedef struct VitaNIDCheckEntry {
	struct VitaNIDCheckEntry *next;
	struct VitaNIDCheckEntry *wnext;
	struct VitaNIDCheckContext *context;
	struct VitaNIDCheckModule *Module;
	struct VitaNIDCheckLibrary *Library;
	char *name;
	int type;
} VitaNIDCheckEntry;

typedef struct VitaNIDCheckLibrary {
	struct VitaNIDCheckLibrary *next;
	struct VitaNIDCheckLibrary *wnext;
	struct VitaNIDCheckContext *context;
	VitaNIDCheckEntry *EntryHead;
	int EntryCounter;
	struct VitaNIDCheckModule *Module;
	char *name;
	int locate;
} VitaNIDCheckLibrary;

typedef struct VitaNIDCheckModule {
	struct VitaNIDCheckModule *next;
	struct VitaNIDCheckModule *wnext;
	struct VitaNIDCheckContext *context;
	VitaNIDCheckLibrary *LibraryHead;
	int LibraryCounter;
	char *name;
} VitaNIDCheckModule;

typedef struct VitaNIDCheckContext {
	struct VitaNIDCheckContext *bypass;
	VitaNIDCheckModule *ModuleHead;
	int ModuleCounter;
	VitaNIDCheckModule *WModule;
	VitaNIDCheckLibrary *WLibrary;
	VitaNIDCheckEntry *WEntry;
	int error;
} VitaNIDCheckContext;

typedef struct VitaNIDCheckParam {
	VitaNIDCheckContext *bypass;
	VitaNIDCheckContext *context;
} VitaNIDCheckParam;

int gDebugLevel;

void chkPrintfLevel(int level, const char *fmt, ...){
	if(level <= gDebugLevel){
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void set_error(VitaNIDCheckContext *context, int error){
	if(context != NULL && context->error == 0){
		context->error = error;
	}
}

int check_bypass(VitaNIDCheckContext *bypass, VitaNIDCheckLibrary *Target, const char *name){

	if(bypass == NULL){
		return -1;
	}

	if(bypass == bypass->bypass){
		return 0;
	}

	VitaNIDCheckModule *Module = bypass->ModuleHead;

	while(Module != NULL){
		if(strcmp(Target->Module->name, Module->name) == 0){

			VitaNIDCheckLibrary *Library = Module->LibraryHead;

			if(Library == NULL){
				chkPrintfLevel(2, "Module %s allowed same name in module level (%s)\n", Module->name, name);
				return 0;
			}

			while(Library != NULL){
				if(strcmp(Target->name, Library->name) == 0){
					VitaNIDCheckEntry *Entry = Target->EntryHead;
					if(Entry == NULL){
						chkPrintfLevel(2, "Module %s allowed same name in library level (%s)\n", Module->name, name);
						return 0;
					}

					while(Entry != NULL){
						if(strcmp(Entry->name, name) == 0){
							chkPrintfLevel(2, "Module %s allowed same name (%s)\n", Module->name, name);
							return 0;
						}
						Entry = Entry->next;
					}
				}
				Library = Library->next;
			}
		}
		Module = Module->next;
	}

	return -1;
}

int check_entry(const char *name, VitaNIDCheckLibrary *Library, int type){

	VitaNIDCheckEntry *Entry;

	if(Library == NULL){
		return -1;
	}

	// Library is cannot same name entry
	{
		Entry = Library->EntryHead;

		while(Entry != NULL){
			if(strcmp(name, Entry->name) == 0){
				chkPrintfLevel(1, "%s is has already in %s::%s\n", name, Entry->Module->name, Entry->Library->name);
				set_error(Library->context, VITASDK_NID_CHK_ERROR_DUPLICATE_NAME);
				return -1;
			}
			Entry = Entry->next;
		}
	}

	{
		Entry = Library->context->WEntry;

		while(Entry != NULL){
			if(strcmp(name, Entry->name) == 0){

				chkPrintfLevel(2,
					"Found same name : %s::%s::%s (type=%d locate=%d) %s::%s::%s (type=%d locate=%d)\n",
					Library->Module->name, Library->name, name, type, Library->locate,
					Entry->Module->name, Entry->Library->name, Entry->name, Entry->type, Entry->Library->locate
				);

				if(Entry->type != type){
					chkPrintfLevel(1, "%s::%s::%s (%d) has a type difference with %s::%s::%s (%d)\n", Library->Module->name, Library->name, name, type, Entry->Module->name, Entry->Library->name, Entry->name, Entry->type);
					set_error(Library->context, VITASDK_NID_CHK_ERROR_DIFF_TYPE);
					return -1;
				}

				/* for
				* SceSysmem::SceSysclibForDriver::memset
				* SceLibc::SceLibc::memset
				*/
				if(Entry->Library->locate == Library->locate && check_bypass(Library->context->bypass, Library, name) < 0){
					chkPrintfLevel(1, "%s::%s::%s is has already in %s::%s\n", Library->Module->name, Library->name, name, Entry->Module->name, Entry->Library->name);
					set_error(Library->context, VITASDK_NID_CHK_ERROR_DUPLICATE_NAME);
					return -1;
				}
			}
			Entry = Entry->wnext;
		}
	}

	return 0;
}

int process_function_entry(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library){

	if(is_scalar(parent) == 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	const char *left = parent->data.scalar.value;

	if(check_entry(left, Library, ENTRY_TYPE_FUNCTION) < 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_CHECK_FAILED);
		return -1;
	}

	if(Library->EntryHead != NULL && Library->EntryHead->type == ENTRY_TYPE_FUNCTION && strcmp(Library->EntryHead->name, left) > 0){
		chkPrintfLevel(1, "Bad sort %s at line %d on %s::%s\n", left, parent->position.line, Library->Module->name, Library->name);
		chkPrintfLevel(1, "Prev ent %s\n", Library->EntryHead->name);
		set_error(Library->context, VITASDK_NID_CHK_ERROR_BAD_SORT);
		return -1;
	}

	VitaNIDCheckEntry *Entry;

	Entry = malloc(sizeof(*Entry));
	if(Entry == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	memset(Entry, 0, sizeof(*Entry));

	Entry->next    = Library->EntryHead;
	Entry->wnext   = Library->context->WEntry;
	Entry->context = Library->context;
	Entry->Module  = Library->Module;
	Entry->Library = Library;
	Entry->name    = strdup(left);
	Entry->type    = ENTRY_TYPE_FUNCTION;

	Library->EntryHead     = Entry;
	Library->EntryCounter += 1;
	Library->context->WEntry = Entry;

	if(Entry->name == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	return 0;
}

int process_variable_entry(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library){

	if(is_scalar(parent) == 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	const char *left = parent->data.scalar.value;

	if(check_entry(left, Library, ENTRY_TYPE_VARUABLE) < 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_CHECK_FAILED);
		return -1;
	}

	if(Library->EntryHead != NULL && Library->EntryHead->type == ENTRY_TYPE_VARUABLE && strcmp(Library->EntryHead->name, left) > 0){
		chkPrintfLevel(1, "Bad sort %s at line %d on %s::%s\n", left, parent->position.line, Library->Module->name, Library->name);
		chkPrintfLevel(1, "Prev ent %s\n", Library->EntryHead->name);
		set_error(Library->context, VITASDK_NID_CHK_ERROR_BAD_SORT);
		return -1;
	}

	VitaNIDCheckEntry *Entry;

	Entry = malloc(sizeof(*Entry));
	if(Entry == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	memset(Entry, 0, sizeof(*Entry));

	Entry->next    = Library->EntryHead;
	Entry->wnext   = Library->context->WEntry;
	Entry->context = Library->context;
	Entry->Module  = Library->Module;
	Entry->Library = Library;
	Entry->name    = strdup(left);
	Entry->type    = ENTRY_TYPE_VARUABLE;

	Library->EntryHead     = Entry;
	Library->EntryCounter += 1;
	Library->context->WEntry = Entry;

	if(Entry->name == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	return 0;
}

int process_library_info(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library){

	if(is_scalar(parent) == 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	int res;
	const char *left = parent->data.scalar.value;

	if(strcmp(left, "kernel") == 0){

		const char *right = child->data.scalar.value;

		if(strcmp(right, "false") == 0){
			Library->locate = LIBRARY_LOCATE_USERMODE;
		}else if(strcmp(right, "true") == 0){
			Library->locate = LIBRARY_LOCATE_KERNEL;
		}else{
			set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
			return -1;
		}

	}else if(strcmp(left, "functions") == 0){
		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_function_entry, Library);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
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
			set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}

	return 0;
}

int process_library_name(yaml_node *parent, yaml_node *child, VitaNIDCheckModule *Module){

	if(is_scalar(parent) == 0){
		set_error(Module->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	const char *left = parent->data.scalar.value;

	VitaNIDCheckLibrary *Library;

	Library = malloc(sizeof(*Library));
	if(Library == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	memset(Library, 0, sizeof(*Library));

	Library->next    = Module->LibraryHead;
	Library->context = Module->context;
	Library->Module  = Module;
	Library->name    = strdup(left);

	Module->LibraryHead     = Library;
	Module->LibraryCounter += 1;

	if(Library->name == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_library_info, Library) < 0){
		return -1;
	}

	return 0;
}

int process_module_info(yaml_node *parent, yaml_node *child, VitaNIDCheckModule *Module){

	if(is_scalar(parent) == 0){
		set_error(Module->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	int res;
	const char *left = parent->data.scalar.value;

	if(is_scalar(parent) == 0){
		set_error(Module->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	if(strcmp(left, "libraries") == 0){

		if(is_mapping(child) != 0){
			res = yaml_iterate_mapping(child, (mapping_functor)process_library_name, Module);
		}else if(is_scalar(child) != 0){
			res = 0;
		}else{
			set_error(Module->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
			res = -1;
		}

		if(res < 0){
			return -1;
		}
	}

	return 0;
}

int process_module_name(yaml_node *parent, yaml_node *child, VitaNIDCheckContext *context){

	if(is_scalar(parent) == 0){
		set_error(context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	const char *left = parent->data.scalar.value;

	VitaNIDCheckModule *Module;

	Module = malloc(sizeof(*Module));
	if(Module == NULL){
		set_error(context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	memset(Module, 0, sizeof(*Module));

	Module->next    = context->ModuleHead;
	Module->context = context;
	Module->name    = strdup(left);

	context->ModuleHead     = Module;
	context->ModuleCounter += 1;

	if(Module->name == NULL){
		set_error(context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_info, Module) < 0){
		return -1;
	}

	return 0;
}

int process_db_top(yaml_node *parent, yaml_node *child, VitaNIDCheckContext *context){

	if(is_scalar(parent) == 0){
		set_error(context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	const char *left = parent->data.scalar.value;

	if(strcmp(left, "modules") == 0){
		if(is_mapping(child) != 0 && yaml_iterate_mapping(child, (mapping_functor)process_module_name, context) < 0){
			return -1;
		}
	}

	return 0;
}

int parse_nid_db_yaml(yaml_document *doc, VitaNIDCheckContext *context){

	if(is_mapping(doc) == 0){
		printf("error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		set_error(context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	return yaml_iterate_mapping(doc, (mapping_functor)process_db_top, context);
}

int add_nid_db_by_fp(VitaNIDCheckContext *context, FILE *fp){

	int res;
	yaml_error error;
	yaml_tree *tree;

	memset(&error, 0, sizeof(error));

	tree = parse_yaml_stream(fp, &error);
	if(tree == NULL){
		printf("error: %s\n", error.problem);
		free(error.problem);
		set_error(context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	res = parse_nid_db_yaml(tree->docs[0], context);

	free_yaml_tree(tree);

	if(res < 0){
		return res;
	}

	return 0;
}

int add_nid_db(VitaNIDCheckContext *context, const char *path){

	int res;

	FILE *fp = fopen(path, "r");
	if(fp == NULL){
		set_error(context, VITASDK_NID_CHK_ERROR_OPEN_FAILED);
		return -1;
	}

	res = add_nid_db_by_fp(context, fp);

	fclose(fp);
	fp = NULL;

	if(res < 0){
		return -1;
	}

	return 0;
}

void free_context(VitaNIDCheckContext *context){

	VitaNIDCheckModule *Module, *ModuleNext;

	if(context == NULL){
		return;
	}

	Module = context->ModuleHead;

	while(Module != NULL){

		ModuleNext = Module->next;

		VitaNIDCheckLibrary *Library = Module->LibraryHead, *LibraryNext;

		while(Library != NULL){

			LibraryNext = Library->next;

			VitaNIDCheckEntry *Entry = Library->EntryHead, *EntryNext;

			while(Entry != NULL){
				EntryNext = Entry->next;
				free(Entry);
				Entry = EntryNext;
			}

			free(Library);
			Library = LibraryNext;
		}

		free(Module);
		Module = ModuleNext;
	}
}

int db_add_callback(FSListEntry *ent, void *argp){

	int res;
	VitaNIDCheckParam *param = (VitaNIDCheckParam *)argp;

	chkPrintfLevel(2, "%s\n", ent->path_full);

	res = add_nid_db(param->context, ent->path_full);
	if(res < 0){
		return -1;
	}

	return 0;
}

int db_top_list_callback(FSListEntry *ent, void *argp){

	int res;
	FSListEntry *nid_db_list = NULL;
	VitaNIDCheckParam *param = (VitaNIDCheckParam *)argp;

	if(ent->isDir == 0){
		chkPrintfLevel(1, "ignored (%s)\n", ent->path_full);
		return 0; // Ignored file
	}

	res = fs_list_init(&nid_db_list, ent->path_full, NULL, NULL);
	if(res >= 0){
		chkPrintfLevel(1, "check (%s)\n", ent->path_full);

		VitaNIDCheckContext context;

		memset(&context, 0, sizeof(context));
		context.bypass = param->bypass;

		param->context = &context;

		res = fs_list_execute(nid_db_list->child, db_add_callback, param);

		free_context(&context);

		if(res < 0 || context.error < 0){
			chkPrintfLevel(1, "check failed (0x%X)\n", context.error);
		}else{
			chkPrintfLevel(1, "check completed\n");
		}
	}
	fs_list_fini(nid_db_list);
	nid_db_list = NULL;

	if(res < 0){
		return res;
	}

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

	int res;
	VitaNIDCheckParam param;
	VitaNIDCheckContext bypass;

	const char *dbg = find_item(argc, argv, "-dbg=");
	const char *bypass_path = find_item(argc, argv, "-bypass=");
	const char *dbdirver = find_item(argc, argv, "-dbdirver=");

	if(argc <= 1 || dbdirver == NULL){
		printf("usage: %s [-dbg=(debug|trace)] [-bypass=./path/to/bypass.yml] [-dbdirver=./path/to/db_dir]\n", "vita-nid-check");
		return EXIT_FAILURE;
	}

	gDebugLevel = 0;

	if(dbg != NULL){
		if(strcmp(dbg, "debug") == 0){
			gDebugLevel = 1;
		}else if(strcmp(dbg, "trace") == 0){
			gDebugLevel = 2;
		}
	}

	memset(&param, 0, sizeof(param));

	if(bypass_path != NULL){
		chkPrintfLevel(2, "bypass: %s\n", bypass_path);

		memset(&bypass, 0, sizeof(bypass));

		bypass.bypass = &bypass;

		res = add_nid_db(&bypass, bypass_path);
		if(res >= 0){
			bypass.bypass = NULL;
			param.bypass = &bypass;
		}
	}

	FSListEntry *nid_db_list = NULL;

	res = fs_list_init_with_depth(&nid_db_list, dbdirver, 0, NULL, NULL);
	if(res >= 0){
		res = fs_list_execute(nid_db_list->child, db_top_list_callback, &param);
	}
	fs_list_fini(nid_db_list);
	nid_db_list = NULL;

	free_context(&bypass);

	if(res < 0){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
