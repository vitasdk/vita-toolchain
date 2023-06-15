/*
 * fs_list
 * Copyright (C) 2022-2023, Princess of Sleeping
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _FS_LIST_H_
#define _FS_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <dirent.h>


typedef struct FSListEntry {
	struct FSListEntry *parent;
	struct FSListEntry *child;
	struct FSListEntry *next;
	char *path_full;
	const char *name;
	int nChild;
	int isDir;
	DIR *dd;
} FSListEntry;

int fs_list_init(FSListEntry **ppEnt, const char *path, int *pnDir, int *pnFile);
int fs_list_init_with_depth(FSListEntry **ppEnt, const char *path, unsigned int depth, int *pnDir, int *pnFile);
int fs_list_fini(FSListEntry *pEnt);

int fs_list_execute(FSListEntry *pEnt, int (* callback)(FSListEntry *ent, void *argp), void *argp);
int fs_list_execute2(FSListEntry *pEnt, int (* callback)(FSListEntry *ent, void *argp), void *argp);

FSListEntry *fs_list_search_entry_by_name(FSListEntry *pEnt, const char *name);
FSListEntry *fs_list_search_entry_by_path(FSListEntry *pEnt, const char *path);


typedef struct FileControlParam {
	int pos;
	const char *output;
} FileControlParam;


#ifdef __cplusplus
}
#endif

#endif /* _FS_LIST_H_ */
