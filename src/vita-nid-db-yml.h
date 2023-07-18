
#ifndef _VITA_NID_DB_YML_H_
#define _VITA_NID_DB_YML_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VitaNIDCallbacks {
	size_t size;
	int (* database_version)(const char *version, void *argp);
	int (* database_firmware)(const char *firmware, void *argp);
	int (* module_name)(const char *name, void *argp);
	int (* module_fingerprint)(uint32_t fingerprint, void *argp);
	int (* library_name)(const char *name, void *argp);
	int (* library_stubname)(const char *name, void *argp);
	int (* library_version)(uint32_t version, void *argp);
	int (* library_nid)(uint32_t nid, void *argp);
	int (* library_privilege)(const char *privilege, void *argp);
	int (* entry_function)(const char *name, uint32_t nid, void *argp);
	int (* entry_variable)(const char *name, uint32_t nid, void *argp);
} VitaNIDCallbacks;

int g_VitaNIDCallbacks_register(const VitaNIDCallbacks *pVitaNIDCallbacks);

int add_nid_db_by_fp(FILE *fp, void *argp);
int add_nid_db_by_path(const char *path, void *argp);

#ifdef __cplusplus
}
#endif

#endif /* _VITA_NID_DB_YML_H_ */
