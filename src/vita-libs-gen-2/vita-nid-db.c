

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vita-nid-db.h"


void db_new_context(DBContext **result){

	DBContext *context;

	context = malloc(sizeof(*context));
	if(context != NULL){
		context->Firmware.next = (DBFirmware *)&(context->Firmware);
		context->Firmware.prev = (DBFirmware *)&(context->Firmware);
		context->pFirmware = NULL;
		context->pModule   = NULL;
		context->pLibrary  = NULL;
	}

	*result = context;
}

void db_new_firmware(DBContext *context, uint32_t firmware, DBFirmware **result){

	DBContext *ctx;
	DBFirmware *fw, *tail;

	*result = NULL;

	if(context != NULL){
		ctx = context;

		fw = malloc(sizeof(*fw));

		tail = ctx->Firmware.prev;

		fw->next = tail->next;
		fw->prev = tail;
		fw->firmware = firmware;
		fw->Module.next = (DBModule *)&(fw->Module);
		fw->Module.prev = (DBModule *)&(fw->Module);

		tail->next->prev = fw;
		tail->next = fw;

		*result = fw;
	}
}

void db_search_firmware(DBContext *context, uint32_t firmware, DBFirmware **result){

	DBContext *ctx;
	DBFirmware *fw, *tail;

	*result = NULL;

	if(context != NULL){
		ctx = context;

		fw = ctx->Firmware.next;
		while(fw != (DBFirmware *)&(ctx->Firmware)){
			if(fw->firmware == firmware){
				*result = fw;
				break;
			}

			fw = fw->next;
		}
	}
}

void db_search_or_new_firmware(DBContext *context, uint32_t firmware, DBFirmware **result){

	db_search_firmware(context, firmware, result);

	if((*result) == NULL){
		db_new_firmware(context, firmware, result);
	}
}

void db_new_module(DBFirmware *fw, const char *name, DBModule **result){

	DBContext *ctx;
	DBModule *module, *tail;

	*result = NULL;

	if(fw != NULL){
		module = malloc(sizeof(*module));

		tail = ((DBFirmware *)fw)->Module.prev;

		module->next = tail->next;
		module->prev = tail;
		module->name = strdup(name);
		module->fingerprint = 0;
		module->Library.next = (DBLibrary *)&(module->Library);
		module->Library.prev = (DBLibrary *)&(module->Library);
		module->firmware = fw;

		tail->next->prev = module;
		tail->next = module;

		*result = module;
	}
}

void db_search_module(DBFirmware *fw, const char *name, DBModule **result){

	DBModule *module;

	*result = NULL;

	if(fw != NULL){
		module = ((DBFirmware *)fw)->Module.next;
		while(module != (DBModule *)&(((DBFirmware *)fw)->Module)){
			if(strcmp(module->name, name) == 0){
				*result = module;
				break;
			}

			module = module->next;
		}
	}
}

void db_search_or_new_module(DBFirmware *fw, const char *name, DBModule **result){

	db_search_module(fw, name, result);

	if((*result) == NULL){
		db_new_module(fw, name, result);
	}
}


void db_new_library(DBModule *module, const char *name, DBLibrary **result){

	DBLibrary *library, *tail;

	*result = NULL;

	if(module != NULL){

		library = malloc(sizeof(*library));

		tail = ((DBModule *)module)->Library.prev;

		library->next = tail->next;
		library->prev = tail;
		library->name = strdup(name);
		library->stubname = NULL;
		library->version = 0;
		library->nid = 0;
		library->privilege = LIBRARY_PRIVILEGE_NONE;
		library->Function.next = (DBEntry *)&(library->Function);
		library->Function.prev = (DBEntry *)&(library->Function);
		library->Variable.next = (DBEntry *)&(library->Variable);
		library->Variable.prev = (DBEntry *)&(library->Variable);
		library->module = module;
		library->firmware = ((DBModule *)module)->firmware;

		tail->next->prev = library;
		tail->next = library;

		*result = library;
	}
}

void db_search_library(DBModule *module, const char *name, DBLibrary **result){

	DBLibrary *library;

	*result = NULL;

	if(module != NULL){

		library = ((DBModule *)module)->Library.next;
		while(library != (DBLibrary *)&(((DBModule *)module)->Library)){
			if(strcmp(library->name, name) == 0){
				*result = library;
				break;
			}

			library = library->next;
		}
	}
}

void db_search_or_new_library(DBModule *module, const char *name, DBLibrary **result){

	db_search_library(module, name, result);

	if((*result) == NULL){
		db_new_library(module, name, result);
	}
}


void db_new_entry_function(DBLibrary *library, const char *name, DBEntry **result){

	DBEntry *entry;

	*result = NULL;

	entry = malloc(sizeof(*entry));

	DBEntry *tail = ((DBLibrary *)library)->Function.prev;

	entry->next = tail->next;
	entry->prev = tail;
	entry->name = strdup(name);
	entry->nid = 0;
	entry->type = ENTRY_TYPE_FUNCTION;
	entry->library = library;
	entry->module = ((DBLibrary *)library)->module;
	entry->firmware = ((DBLibrary *)library)->firmware;

	tail->next->prev = entry;
	tail->next = entry;

	*result = entry;
}

void db_search_entry_function(DBLibrary *library, const char *name, DBEntry **result){

	DBEntry *entry;

	*result = NULL;

	entry = ((DBLibrary *)library)->Function.next;
	while(entry != (DBEntry *)&(((DBLibrary *)library)->Function)){

		if(strcmp(entry->name, name) == 0){
			*result = entry;
			break;
		}

		entry = entry->next;
	}
}

void db_search_or_new_entry_function(DBLibrary *library, const char *name, DBEntry **result){
	db_search_entry_function(library, name, result);
	if((*result) == NULL){
		db_new_entry_function(library, name, result);
	}
}

void db_new_entry_variable(DBLibrary *library, const char *name, DBEntry **result){

	DBEntry *entry;

	*result = NULL;

	entry = malloc(sizeof(*entry));

	DBEntry *tail = ((DBLibrary *)library)->Variable.prev;

	entry->next = tail->next;
	entry->prev = tail;
	entry->name = strdup(name);
	entry->nid = 0;
	entry->type = ENTRY_TYPE_VARUABLE;
	entry->library = library;
	entry->module = ((DBLibrary *)library)->module;
	entry->firmware = ((DBLibrary *)library)->firmware;

	tail->next->prev = entry;
	tail->next = entry;

	*result = entry;
}

void db_search_entry_variable(DBLibrary *library, const char *name, DBEntry **result){

	DBEntry *entry;

	*result = NULL;

	entry = ((DBLibrary *)library)->Variable.next;
	while(entry != (DBEntry *)&(((DBLibrary *)library)->Variable)){

		if(strcmp(entry->name, name) == 0){
			*result = entry;
			break;
		}

		entry = entry->next;
	}
}

void db_search_or_new_entry_variable(DBLibrary *library, const char *name, DBEntry **result){
	db_search_entry_variable(library, name, result);
	if((*result) == NULL){
		db_new_entry_variable(library, name, result);
	}
}


int db_execute_fw_vector(DBContext *context, int (* callback)(DBFirmware *fw, void *argp), void *argp){

	int res;
	DBFirmware *fw;

	if(context != NULL){
		fw = ((DBContext *)context)->Firmware.next;
		while(fw != (DBFirmware *)&(((DBContext *)context)->Firmware)){

			DBFirmware *next = fw->next;

			res = callback(fw, argp);
			if(res < 0){
				return -1;
			}

			fw = next;
		}
	}

	return 0;
}

int db_execute_module_vector(DBFirmware *fw, int (* callback)(DBModule *module, void *argp), void *argp){

	int res;
	DBModule *module;

	if(fw != NULL){
		module = ((DBFirmware *)fw)->Module.next;
		while(module != (DBModule *)&(((DBFirmware *)fw)->Module)){

			DBModule *next = module->next;

			res = callback(module, argp);
			if(res < 0){
				return -1;
			}

			module = next;
		}
	}

	return 0;
}

int db_execute_library_vector(DBModule *module, int (* callback)(DBLibrary *library, void *argp), void *argp){

	int res;
	DBLibrary *library;

	if(module != NULL){
		library = ((DBModule *)module)->Library.next;
		while(library != (DBLibrary *)&(((DBModule *)module)->Library)){

			DBLibrary *next = library->next;

			res = callback(library, argp);
			if(res < 0){
				return -1;
			}

			library = next;
		}
	}

	return 0;
}

int db_execute_function_vector(DBLibrary *library, int (* callback)(DBEntry *entry, void *argp), void *argp){

	int res;
	DBEntry *entry;

	if(library != NULL){
		entry = ((DBLibrary *)library)->Function.next;
		while(entry != (DBEntry *)&(((DBLibrary *)library)->Function)){

			DBEntry *next = entry->next;

			res = callback(entry, argp);
			if(res < 0){
				return -1;
			}

			entry = next;
		}
	}

	return 0;
}

int db_execute_variable_vector(DBLibrary *library, int (* callback)(DBEntry *entry, void *argp), void *argp){

	int res;
	DBEntry *entry;

	if(library != NULL){
		entry = ((DBLibrary *)library)->Variable.next;
		while(entry != (DBEntry *)&(((DBLibrary *)library)->Variable)){

			DBEntry *next = entry->next;

			res = callback(entry, argp);
			if(res < 0){
				return -1;
			}

			entry = next;
		}
	}

	return 0;
}


static int db_free_fw_callback(DBFirmware *fw, void *argp){
	db_free_fw(fw);
	return 0;
}

void db_free_context(DBContext *context){

	db_execute_fw_vector(context, db_free_fw_callback, NULL);

	free(context);
}

static int db_free_module_callback(DBModule *module, void *argp){
	db_free_module(module);
	return 0;
}

void db_free_fw(DBFirmware *fw){

	fw->next->prev = fw->prev;
	fw->prev->next = fw->next;

	db_execute_module_vector(fw, db_free_module_callback, NULL);

	free(fw);
}

static int db_free_library_callback(DBLibrary *library, void *argp){
	db_free_library(library);
	return 0;
}

void db_free_module(DBModule *module){

	module->next->prev = module->prev;
	module->prev->next = module->next;

	db_execute_library_vector(module, db_free_library_callback, NULL);

	free(module->name);
	free(module);
}

static int db_free_entry_callback(DBEntry *entry, void *argp){
	db_free_entry(entry);
	return 0;
}

void db_free_library(DBLibrary *library){

	library->next->prev = library->prev;
	library->prev->next = library->next;

	db_execute_function_vector(library, db_free_entry_callback, NULL);
	db_execute_variable_vector(library, db_free_entry_callback, NULL);

	free(library->name);
	free(library->stubname);
	free(library);
}

void db_free_entry(DBEntry *entry){
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	free(entry->name);
	free(entry);
}




void _vita_nid_db_search_stub_by_name(StubContext *ctx, const char *name, NidStub **stub){

	NidStub *current;

	*stub = NULL;

	current = ctx->Stub.next;

	while(current != (NidStub *)&(ctx->Stub)){
		if(ctx->firmware == current->firmware && strcmp(current->name, name) == 0){
			*stub = current;
			break;
		}
		current = current->next;
	}
}

void _vita_nid_db_search_or_create_stub_by_name(StubContext *ctx, const char *name, NidStub **stub){

	NidStub *current, *tail;

	char libname[0x100 + 0x10];

	if(ctx->firmware != 0 && (ctx->firmware & ~0xFFF) != (0x3600011 & ~0xFFF)){
		if((ctx->firmware & 0xFF000000) == 0){ // 0.990/0.995
			snprintf(libname, sizeof(libname), "%s_0%03X", name, ctx->firmware >> 12);
		}else if(((ctx->firmware >> 12) & 0xF) != 0){ // 1.692
			snprintf(libname, sizeof(libname), "%s_%03X", name, ctx->firmware >> 12);
		}else{
			snprintf(libname, sizeof(libname), "%s_%03X", name, ctx->firmware >> 16);
		}
	}else{
		snprintf(libname, sizeof(libname), "%s", name);
	}

	_vita_nid_db_search_stub_by_name(ctx, libname, stub);

	if((*stub) == NULL){
		current = (NidStub *)malloc(sizeof(*current));
		current->next = NULL;
		current->prev = NULL;
		current->firmware = ctx->firmware;

		current->name = strdup(libname);
		current->LibStub.next = (_VitaNIDLibStub *)&(current->LibStub);
		current->LibStub.prev = (_VitaNIDLibStub *)&(current->LibStub);
		current->context = ctx;

		tail = ctx->Stub.prev;

		current->next = tail->next;
		current->prev = tail;

		tail->next->prev = current;
		tail->next = current;

		*stub = current;
	}
}

void _vita_nid_db_stub_push_library(NidStub *stub, DBLibrary *library){

	_VitaNIDLibStub *tail = stub->LibStub.prev;

	_VitaNIDLibStub *libstub;

	libstub = (_VitaNIDLibStub *)malloc(sizeof(*libstub));

	libstub->next = tail->next;
	libstub->prev = tail;
	libstub->nid_stub = stub;
	libstub->library = library;

	tail->next->prev = libstub;
	tail->next = libstub;
}

int libstub_execute_vector(NidStub *nid_stub, int (* callback)(_VitaNIDLibStub *libstub, void *argp), void *argp){

	_VitaNIDLibStub *libstub = nid_stub->LibStub.next;
	while(libstub != (_VitaNIDLibStub *)&(nid_stub->LibStub)){
		_VitaNIDLibStub *next = libstub->next;
		int res = callback(libstub, argp);
		if(res < 0){
			return res;
		}

		libstub = next;
	}

	return 0;
}

int stub_execute_vector(StubContext *context, int (* callback)(NidStub *nid_stub, void *argp), void *argp){

	NidStub *nid_stub = context->Stub.next;
	while(nid_stub != (NidStub *)&(context->Stub)){
		NidStub *next = nid_stub->next;
		int res = callback(nid_stub, argp);
		if(res < 0){
			return res;
		}

		nid_stub = next;
	}

	return 0;
}
