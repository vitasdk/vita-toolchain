
#ifndef _VITA_NID_DB_H_
#define _VITA_NID_DB_H_

#include <stdint.h>

typedef struct VitaNIDLibrary VitaNIDLibrary;
typedef struct VitaNIDModule VitaNIDModule;
typedef struct VitaNIDDB VitaNIDDB;

typedef struct VitaNIDEntry {
	struct VitaNIDEntry *next;
	struct VitaNIDEntry *prev;
	char *name;
	uint32_t nid;
	VitaNIDDB *db;
	VitaNIDModule *module;
	VitaNIDLibrary *library;
} VitaNIDEntry;

typedef struct VitaNIDLibrary {
	struct VitaNIDLibrary *next;
	struct VitaNIDLibrary *prev;
	char *name;
	char *stubname;
	uint32_t nid;
	uint32_t version;
	uint32_t firmware;
	int is_kernel;
	struct {
		VitaNIDEntry *next;
		VitaNIDEntry *prev;
	} Function;
	struct {
		VitaNIDEntry *next;
		VitaNIDEntry *prev;
	} Variable;
	VitaNIDDB *db;
	VitaNIDModule *module;
} VitaNIDLibrary;

typedef struct VitaNIDModule {
	struct VitaNIDModule *next;
	struct VitaNIDModule *prev;
	char *name;
	uint32_t fingerprint;
	uint32_t firmware;
	struct {
		VitaNIDLibrary *next;
		VitaNIDLibrary *prev;
	} Library;
	VitaNIDDB *db;
} VitaNIDModule;

typedef struct VitaNIDLibStub {
	struct VitaNIDLibStub *next;
	struct VitaNIDLibStub *prev;
	VitaNIDLibrary *library;
} VitaNIDLibStub;

typedef struct VitaNIDStub {
	struct VitaNIDStub *next;
	struct VitaNIDStub *prev;
	uint32_t firmware;
	char *name;
	struct {
		VitaNIDLibStub *next;
		VitaNIDLibStub *prev;
	} LibStub;
	VitaNIDDB *db;
} VitaNIDStub;

typedef struct VitaNIDDB {
	uint32_t firmware;
	struct {
		VitaNIDModule *next;
		VitaNIDModule *prev;
	} Module;
	struct {
		VitaNIDStub *next;
		VitaNIDStub *prev;
	} Stub;
} VitaNIDDB;


#endif /* _VITA_NID_DB_H_ */
