
#ifndef _DB_MAIN_H_
#define _DB_MAIN_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _NIDDbBypassEntry {
	struct _NIDDbBypassEntry *next;
	struct _NIDDbBypassEntry *prev;
	char *name;
} NIDDbBypassEntry;

typedef struct _NIDDbBypassLibrary {
	struct _NIDDbBypassLibrary *next;
	struct _NIDDbBypassLibrary *prev;
	char *name;
	struct {
		NIDDbBypassEntry *next;
		NIDDbBypassEntry *prev;
	} entry;
} NIDDbBypassLibrary;

typedef struct _NIDDbBypass {
	struct {
		NIDDbBypassLibrary *next;
		NIDDbBypassLibrary *prev;
	} library;
} NIDDbBypass;

NIDDbBypassEntry *nid_db_bypass_search_entry_by_name(NIDDbBypassLibrary *library, const char *name);
NIDDbBypassLibrary *nid_db_bypass_search_library(NIDDbBypass *bypass, const char *name);

int load_nid_db_bypass_by_fp(FILE *fp, void *argp);
int load_nid_db_bypass_by_path(const char *path, void *argp);


#ifdef __cplusplus
}
#endif

#endif /* _DB_MAIN_H_ */
