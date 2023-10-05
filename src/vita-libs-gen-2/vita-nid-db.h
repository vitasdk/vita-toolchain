
#ifndef _VITA_NID_DB_H_
#define _VITA_NID_DB_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


#define ENTRY_TYPE_NONE     (0)
#define ENTRY_TYPE_FUNCTION (1)
#define ENTRY_TYPE_VARUABLE (2)

#define LIBRARY_LOCATE_NONE     (0)
#define LIBRARY_LOCATE_USERMODE (1)
#define LIBRARY_LOCATE_KERNEL   (2)

typedef struct DBEntry {
	struct DBEntry *next;
	struct DBEntry *prev;
	char *name;
	uint32_t nid;
	int type;
	struct DBLibrary *library;
	struct DBModule *module;
	struct DBFirmware *firmware;
} DBEntry;

typedef struct DBLibrary {
	struct DBLibrary *next;
	struct DBLibrary *prev;
	char *name;
	char *stubname;
	uint32_t version;
	uint32_t nid;
	int privilege;
	struct {
		DBEntry *next;
		DBEntry *prev;
	} Function;
	struct {
		DBEntry *next;
		DBEntry *prev;
	} Variable;
	struct DBModule *module;
	struct DBFirmware *firmware;
} DBLibrary;

typedef struct DBModule {
	struct DBModule *next;
	struct DBModule *prev;
	char *name;
	uint32_t fingerprint;
	struct {
		DBLibrary *next;
		DBLibrary *prev;
	} Library;
	struct DBFirmware *firmware;
} DBModule;

typedef struct DBFirmware {
	struct DBFirmware *next;
	struct DBFirmware *prev;
	uint32_t firmware;
	struct {
		DBModule *next;
		DBModule *prev;
	} Module;
} DBFirmware;

typedef struct DBContext {
	struct {
		DBFirmware *next;
		DBFirmware *prev;
	} Firmware;
	DBFirmware *pFirmware;
	DBModule *pModule;
	DBLibrary *pLibrary;
} DBContext;

void db_new_context(DBContext **result);

void db_new_firmware(DBContext *context, uint32_t firmware, DBFirmware **result);
void db_search_firmware(DBContext *context, uint32_t firmware, DBFirmware **result);
void db_search_or_new_firmware(DBContext *context, uint32_t firmware, DBFirmware **result);

void db_new_module(DBFirmware *fw, const char *name, DBModule **result);
void db_search_module(DBFirmware *fw, const char *name, DBModule **result);
void db_search_or_new_module(DBFirmware *fw, const char *name, DBModule **result);

void db_new_library(DBModule *module, const char *name, DBLibrary **result);
void db_search_library(DBModule *module, const char *name, DBLibrary **result);
void db_search_or_new_library(DBModule *module, const char *name, DBLibrary **result);

#define LIBRARY_PRIVILEGE_NONE   (0)
#define LIBRARY_PRIVILEGE_USER   (1)
#define LIBRARY_PRIVILEGE_KERNEL (2)

void db_new_entry_function(DBLibrary *library, const char *name, DBEntry **result);
void db_search_entry_function(DBLibrary *library, const char *name, DBEntry **result);
void db_search_or_new_entry_function(DBLibrary *library, const char *name, DBEntry **result);
void db_new_entry_variable(DBLibrary *library, const char *name, DBEntry **result);
void db_search_entry_variable(DBLibrary *library, const char *name, DBEntry **result);
void db_search_or_new_entry_variable(DBLibrary *library, const char *name, DBEntry **result);

int db_execute_fw_vector(DBContext *context, int (* callback)(DBFirmware *fw, void *argp), void *argp);
int db_execute_module_vector(DBFirmware *fw, int (* callback)(DBModule *module, void *argp), void *argp);
int db_execute_library_vector(DBModule *module, int (* callback)(DBLibrary *library, void *argp), void *argp);
int db_execute_function_vector(DBLibrary *library, int (* callback)(DBEntry *entry, void *argp), void *argp);
int db_execute_variable_vector(DBLibrary *library, int (* callback)(DBEntry *entry, void *argp), void *argp);

void db_free_context(DBContext *context);
void db_free_fw(DBFirmware *fw);
void db_free_module(DBModule *module);
void db_free_library(DBLibrary *library);
void db_free_entry(DBEntry *entry);


// TODO: Get better naming.

typedef struct _VitaNIDLibStub {
	struct _VitaNIDLibStub *next;
	struct _VitaNIDLibStub *prev;
	struct NidStub *nid_stub;
	DBLibrary *library;
} _VitaNIDLibStub;

typedef struct NidStub {
	struct NidStub *next;
	struct NidStub *prev;
	uint32_t firmware;
	char *name;
	struct {
		_VitaNIDLibStub *next;
		_VitaNIDLibStub *prev;
	} LibStub;
	struct StubContext *context;
} NidStub;

typedef struct StubContext {
	struct {
		NidStub *next;
		NidStub *prev;
	} Stub;
	uint32_t firmware;
	FILE *fp;
	int ignored_stubname;
} StubContext;

void _vita_nid_db_search_stub_by_name(StubContext *ctx, const char *name, NidStub **stub);
void _vita_nid_db_search_or_create_stub_by_name(StubContext *ctx, const char *name, NidStub **stub);
void _vita_nid_db_stub_push_library(NidStub *stub, DBLibrary *library);
int libstub_execute_vector(NidStub *nid_stub, int (* callback)(_VitaNIDLibStub *libstub, void *argp), void *argp);
int stub_execute_vector(StubContext *context, int (* callback)(NidStub *nid_stub, void *argp), void *argp);


#ifdef __cplusplus
}
#endif

#endif /* _VITA_NID_DB_H_ */
