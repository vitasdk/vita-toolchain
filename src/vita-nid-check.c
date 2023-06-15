

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
	int nid;
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

const char *get_type_string(int type){
	if(type == ENTRY_TYPE_FUNCTION){
		return "function";
	}else if(type == ENTRY_TYPE_VARUABLE){
		return "variable";
	}

	return "unknown";
}

const char *get_locate_string(int locate){
	if(locate == LIBRARY_LOCATE_KERNEL){
		return "kernel";
	}else if(locate == LIBRARY_LOCATE_USERMODE){
		return "user";
	}

	return "unknown";
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

int check_entry(VitaNIDCheckLibrary *Library, int type, const char *name, int nid){

	VitaNIDCheckEntry *Entry;

	if(Library == NULL){
		return -1;
	}

	// RULE: Library is cannot same name entry
	{
		Entry = Library->EntryHead;

		while(Entry != NULL){
			if(strcmp(name, Entry->name) == 0){
				chkPrintfLevel(1, "%s (0x%08X) is has already in %s::%s::%s (0x%08X)\n", name, nid, Entry->Module->name, Entry->Library->name, Entry->name, Entry->nid);
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
					"[%-31s] %s::%s (nid=0x%08X %s %s) %s::%s (nid=0x%08X %s %s)\n",
					name, Library->Module->name, Library->name, nid, get_locate_string(Library->locate), get_type_string(type),
					Entry->Module->name, Entry->Library->name, Entry->nid, get_locate_string(Entry->Library->locate), get_type_string(Entry->type)
				);

				// RULE: Not allowed same name with difference type. (func != var)
				if(Entry->type != type){
					chkPrintfLevel(1,
						"%s::%s::%s has a type difference with %s::%s::%s (%s != %s)\n",
						Library->Module->name, Library->name, name,
						Entry->Module->name, Entry->Library->name, Entry->name, get_type_string(type), get_type_string(Entry->type)
					);
					set_error(Library->context, VITASDK_NID_CHK_ERROR_DIFF_TYPE);
					return -1;
				}

				/* SCE RULE: Not allowed same name with difference nid in module. (but prototype is should be same.)
				 *
				 * SceSysrootForKernel::sceKernelProcessDebugSuspend (0x1247A825)
				 * SceProcessmgrForKernel::sceKernelProcessDebugSuspend (0x6AECE4CD)
				 *
				 * SceThreadmgr::sceKernelGetProcessId (0x9DCB4B7A)
				 * SceThreadmgrForDriver::sceKernelGetProcessId (0x9DCB4B7A)
				 */
				if(nid != Entry->nid && strcmp(Library->Module->name, Entry->Module->name) == 0){
					chkPrintfLevel(1,
						"%s::%s::%s has a NID difference with %s::%s::%s (0x%08X != 0x%08X)\n",
						Library->Module->name, Library->name, name,
						Entry->Module->name, Entry->Library->name, Entry->name, nid, Entry->nid
					);
					set_error(Library->context, VITASDK_NID_CHK_ERROR_DUPLICATE_NAME);
					return -1;
				}

				/* VITASDK RULE: Not allowed same name in same space. (but can bypass it!)
				 *
				 * SceSysmem::SceSysclibForDriver::memset (0x0AB9BF5C)
				 * SceLibc::SceLibc::memset (0x6DC1F0D8)
				 * ScePaf::ScePafStdc::memset (0x75ECC54E)
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

int process_entry(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library, int type){

	if(is_scalar(parent) == 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	int nid;
	const char *left = parent->data.scalar.value;

	if(process_32bit_integer(child, &nid) < 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_INVALID_FORMAT);
		return -1;
	}

	if(check_entry(Library, type, left, nid) < 0){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_CHECK_FAILED);
		return -1;
	}

	if(Library->EntryHead != NULL && Library->EntryHead->type == type && strcmp(Library->EntryHead->name, left) > 0){
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
	Entry->type    = type;
	Entry->nid     = nid;

	Library->EntryHead     = Entry;
	Library->EntryCounter += 1;
	Library->context->WEntry = Entry;

	if(Entry->name == NULL){
		set_error(Library->context, VITASDK_NID_CHK_ERROR_NO_MEMORY);
		return -1;
	}

	return 0;
}

int process_function_entry(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library){
	return process_entry(parent, child, Library, ENTRY_TYPE_FUNCTION);
}

int process_variable_entry(yaml_node *parent, yaml_node *child, VitaNIDCheckLibrary *Library){
	return process_entry(parent, child, Library, ENTRY_TYPE_VARUABLE);
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
		if(res < 0){
			chkPrintfLevel(2, "%s: failed fs_list_execute 0x%X\n", __FUNCTION__, res);
		}
	}else{
		chkPrintfLevel(1, "%s: failed fs_list_init_with_depth 0x%X\n", __FUNCTION__, res);
	}
	fs_list_fini(nid_db_list);
	nid_db_list = NULL;

	free_context(&bypass);

	if(res < 0){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
