

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "vita-nid-bypass.h"
#include "utils/fs_list.h"
#include "vita-libs-gen-2/vita-nid-db.h"
#include "vita-libs-gen-2/vita-nid-db-yml.h"

typedef struct _VitaNIDCheckParam {
	NIDDbBypass bypass;
	struct {
		DBContext *context;
		const char *name;
		uint32_t nid;
		int type;
	} current;
} VitaNIDCheckParam;

unsigned int gDebugLevel = 0;

void chkPrintfLevel(unsigned int level, const char *fmt, ...){
	if(level <= gDebugLevel){
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

int function_callback(DBEntry *entry, void *argp){

	VitaNIDCheckParam *param = argp;

	if(strcmp(param->current.name, entry->name) != 0){
		return 0;
	}

	if(param->current.context->pLibrary->privilege != entry->library->privilege){
		return 0;
	}

	// RULE: Not allowed same name with difference type. (func != var)
	if(entry->type != param->current.type){

		printf("There are entries with the same name of different types\n");
		printf(
			"  %s::%s::%s (nid=0x%08X type=%d)\n",
			entry->module->name,
			entry->library->name,
			entry->name,
			entry->nid,
			entry->type
		);
		printf(
			"  %s::%s::%s (nid=0x%08X type=%d)\n",
			param->current.context->pModule->name,
			param->current.context->pLibrary->name,
			param->current.name,
			param->current.nid,
			param->current.type
		);

		return -1;
	}

	NIDDbBypassLibrary *lib = nid_db_bypass_search_library(&(param->bypass), entry->library->name);
	if(lib != NULL){
		NIDDbBypassEntry *ent = nid_db_bypass_search_entry_by_name(lib, entry->name);
		if(ent != NULL){
			return 0;
		}
	}

	printf("There are entries with the same name\n");
	printf(
		"  %s::%s::%s (0x%08X)\n",
		entry->module->name,
		entry->library->name,
		entry->name, entry->nid
	);
	printf(
		"  %s::%s::%s (0x%08X)\n",
		param->current.context->pModule->name,
		param->current.context->pLibrary->name,
		param->current.name,
		param->current.nid
	);

	return -1;
}

int library_callback(DBLibrary *library, void *argp){

	int res;

	res = db_execute_function_vector(library, function_callback, argp);
	if(res < 0){
		return res;
	}

	res = db_execute_variable_vector(library, function_callback, argp);
	if(res < 0){
		return res;
	}

	return 0;
}

int module_callback(DBModule *module, void *argp){
	return db_execute_library_vector(module, library_callback, argp);
}

int check_in_same_library(DBEntry *entry, void *argp){

	VitaNIDCheckParam *param = argp;

	if(strcmp(param->current.name, entry->name) == 0){
		return -1;
	}

	return 0;
}

int _check_entry(VitaNIDCheckParam *param, const char *name, uint32_t nid, int type){

	int res;

	param->current.name = name;
	param->current.nid  = nid;
	param->current.type = type;

	DBEntry *e;

	if(type == ENTRY_TYPE_FUNCTION){
		e = (DBEntry *)&(param->current.context->pLibrary->Function);
	}else if(type == ENTRY_TYPE_VARUABLE){
		e = (DBEntry *)&(param->current.context->pLibrary->Variable);
	}else{
		printf("internal type error\n");
		return -1;
	}

	if(e != e->prev && strcmp(name, e->prev->name) <= 0){
		chkPrintfLevel(1, "Bad sort %s at module=%s library=%s\n", name, param->current.context->pModule->name, param->current.context->pLibrary->name);
		chkPrintfLevel(1, "Prev ent %s\n", e->prev->name);
		return -1;
	}

	/*
	 * RULE: Library cannot have same name entry
	 */
	res = db_execute_function_vector(param->current.context->pLibrary, check_in_same_library, param);
	if(res < 0){
		return res;
	}

	res = db_execute_variable_vector(param->current.context->pLibrary, check_in_same_library, param);
	if(res < 0){
		return res;
	}

	/*
	 * Stage 2 - lookup all loaded NIDs
	 */
	return db_execute_module_vector(param->current.context->pFirmware, module_callback, param);
}

int database_firmware(const char *firmware, void *argp){

	VitaNIDCheckParam *param = argp;

	db_search_or_new_firmware(param->current.context, 0, &(param->current.context->pFirmware));

	return 0;
}

int module_name(const char *name, void *argp){

	VitaNIDCheckParam *param = argp;

	db_search_or_new_module(param->current.context->pFirmware, name, &(param->current.context->pModule));

	return 0;
}

int library_name(const char *name, void *argp){

	VitaNIDCheckParam *param = argp;

	db_search_or_new_library(param->current.context->pModule, name, &(param->current.context->pLibrary));

	return 0;
}

int library_nid(uint32_t nid, void *argp){

	VitaNIDCheckParam *param = argp;

	param->current.context->pLibrary->nid = nid;

	return 0;
}

int library_privilege(const char *privilege, void *argp){

	VitaNIDCheckParam *param = argp;

	if(strcmp(privilege, "kernel") == 0){
		param->current.context->pLibrary->privilege = LIBRARY_PRIVILEGE_KERNEL;
	}else if(strcmp(privilege, "user") == 0){
		param->current.context->pLibrary->privilege = LIBRARY_PRIVILEGE_USER;
	}else{
		param->current.context->pLibrary->privilege = LIBRARY_PRIVILEGE_NONE;
	}


	return 0;
}

int entry_function(const char *name, uint32_t nid, void *argp){

	VitaNIDCheckParam *param = argp;

	if(strlen(name) == 0){
		return -1;
	}

	int res = _check_entry(param, name, nid, ENTRY_TYPE_FUNCTION);
	if(res < 0){
		return res;
	}

	DBEntry *entry = NULL;

	db_new_entry_function(param->current.context->pLibrary, name, &entry);

	if(entry == NULL){
		printf("internal memory error\n");
		return -1;
	}

	entry->nid = nid;

	return 0;
}

int entry_variable(const char *name, uint32_t nid, void *argp){

	VitaNIDCheckParam *param = argp;

	int res = _check_entry(param, name, nid, ENTRY_TYPE_VARUABLE);
	if(res < 0){
		return res;
	}

	DBEntry *entry = NULL;

	db_new_entry_variable(param->current.context->pLibrary, name, &entry);

	if(entry == NULL){
		printf("internal memory error\n");
		return -1;
	}

	entry->nid = nid;

	return 0;
}

int db_add_callback(FSListEntry *ent, void *argp){

	int res;

	if(ent->isDir != 0){
		return 0;
	}

	chkPrintfLevel(2, "%s\n", ent->path_full);

	res = add_nid_db_by_path(ent->path_full, argp);
	if(res < 0){
		return -1;
	}

	return 0;
}

static const VitaNIDCallbacks nid_check_callbacks = {
	.size               = sizeof(VitaNIDCallbacks),
	.database_version   = NULL,
	.database_firmware  = database_firmware,
	.module_name        = module_name,
	.module_fingerprint = NULL,
	.library_name       = library_name,
	.library_stubname   = NULL,
	.library_version    = NULL,
	.library_nid        = library_nid,
	.library_privilege  = library_privilege,
	.entry_function     = entry_function,
	.entry_variable     = entry_variable
};

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

	const char *dbg = find_item(argc, argv, "-dbg=");
	const char *bypass_path = find_item(argc, argv, "-bypass=");
	const char *dbdirver = find_item(argc, argv, "-dbdirver=");

	if(argc <= 1 || dbdirver == NULL){
		printf("usage: %s [-dbdirver=<./path/to/db_dir>] [-bypass=<./path/to/bypass.yml>] [-dbg=<debug|trace>]\n", "vita-nid-check");
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

	VitaNIDCheckParam param;
	memset(&param, 0, sizeof(param));

	param.bypass.library.next = (NIDDbBypassLibrary *)&(param.bypass.library);
	param.bypass.library.prev = (NIDDbBypassLibrary *)&(param.bypass.library);

	g_VitaNIDCallbacks_register(&nid_check_callbacks);

	if(bypass_path != NULL){
		res = load_nid_db_bypass_by_path(bypass_path, &(param.bypass));
		if(res < 0){
			chkPrintfLevel(1, "failed bypass loading 0x%X for %s\n", res, bypass_path);
			return EXIT_FAILURE;
		}
	}

	FSListEntry *nid_db_list = NULL;

	res = fs_list_init(&nid_db_list, dbdirver, NULL, NULL);
	if(res >= 0){
		db_new_context(&(param.current.context));

		res = fs_list_execute(nid_db_list->child, db_add_callback, &param);

		db_free_context(param.current.context);
		memset(&(param.current), 0, sizeof(param.current));
	}else{
		chkPrintfLevel(1, "%s: failed fs_list_init_with_depth 0x%X\n", __FUNCTION__, res);
	}

	fs_list_fini(nid_db_list);
	nid_db_list = NULL;

	// TODO : free bypass

	if(res < 0){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
