/*
 * fs_list
 * Copyright (C) 2022-2023, Princess of Sleeping
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "fs_list.h"


FSListEntry *fs_list_make_entry(FSListEntry *pCurrent, struct dirent *pDirent){

	char *path;
	size_t path_len;
	FSListEntry *pEnt;

	pEnt = malloc(sizeof(*pEnt));
	if(pEnt == NULL){
		return NULL;
	}

	memset(pEnt, 0, sizeof(*pEnt));

	pEnt->parent = NULL;
	pEnt->child  = NULL;
	pEnt->next   = NULL;

	if(pDirent != NULL){
		path_len = strlen(pCurrent->path_full) + strlen("/") + strlen(pDirent->d_name);
		path     = malloc(path_len + 1);

		snprintf(path, path_len + 1, "%s/%s", pCurrent->path_full, pDirent->d_name);

		pEnt->name = &(path[strlen(pCurrent->path_full) + strlen("/")]);
	}else{
		path_len = strlen(pCurrent->path_full);
		path     = malloc(path_len + 1);

		strncpy(path, pCurrent->path_full, path_len);
	}

	path[path_len] = 0;
	pEnt->path_full = path;

	pEnt->nChild = 0;
	pEnt->dd = NULL;

	return pEnt;
}

int fs_list_free_entry(FSListEntry *pEnt){

	if(NULL == pEnt){
		return 0;
	}

	if(NULL != pEnt->path_full){
		memset(pEnt->path_full, 0, strlen(pEnt->path_full));
		free(pEnt->path_full);
	}

	// sce_paf_memset(pEnt, 0xAA, sizeof(*pEnt));
	free(pEnt);

	return 0;
}

int fs_list_init_core(const char *path, FSListEntry **ppEnt, unsigned int depth, int *pnDir, int *pnFile){

	int res;
	int current_depth, nDir = 0, nFile = 0;
	FSListEntry *pEnt, *current, *child;
	struct stat stat_buf;

	res = stat(path, &stat_buf);
	if(res != 0){
		return -1;
	}

	pEnt = malloc(sizeof(*pEnt));
	if(pEnt == NULL){
		return -1;
	}

	memset(pEnt, 0, sizeof(*pEnt));

	size_t path_len = strlen(path);

	char *path_full = malloc(path_len + 1);

	path_full[path_len] = 0;
	strncpy(path_full, path, path_len);

	pEnt->path_full = path_full;

	if(S_ISDIR(stat_buf.st_mode)){
		pEnt->isDir = 1;
	}

	current = pEnt;

	DIR *dd = opendir(current->path_full);
	if(dd == NULL){
		fs_list_free_entry(pEnt);
		return -1;
	}

	current->dd = dd;

	current_depth = 0;

	struct dirent *dir_ent;

	while(current != NULL){

		dir_ent = readdir(current->dd);
		if(dir_ent != NULL){

			if(strcmp(dir_ent->d_name, ".") != 0 && strcmp(dir_ent->d_name, "..") != 0){
				child = fs_list_make_entry(current, dir_ent);
				if(child != NULL){
					child->parent = current;
					child->next   = current->child;
					current->child = child;
					current->nChild += 1;

					res = stat(child->path_full, &stat_buf);
					if(res != 0){
						return -1;
					}

					if(S_ISDIR(stat_buf.st_mode)){

						child->isDir = 1;

						if((current_depth + 1) <= depth){
							dd = opendir(child->path_full);
							if(dd != NULL){
								child->dd = dd;
								current = child;
							}
							current_depth++;
						}

						nDir++;
					}
					else if(S_ISREG(stat_buf.st_mode)){

						child->isDir = 0;
						nFile++;
					}
				}
			}

		}else{ // No has more entries
			closedir(current->dd);
			current->dd = NULL;
			current = current->parent;
			current_depth--;
		}
	}

	*ppEnt = pEnt;

	if (pnDir != NULL) {
		*pnDir += nDir;
	}

	if (pnFile != NULL) {
		*pnFile += nFile;
	}

	return 0;
}

int fs_list_init(FSListEntry **ppEnt, const char *path, int *pnDir, int *pnFile){
	return fs_list_init_core(path, ppEnt, ~0, pnDir, pnFile);
}

int fs_list_init_with_depth(FSListEntry **ppEnt, const char *path, unsigned int depth, int *pnDir, int *pnFile){
	return fs_list_init_core(path, ppEnt, depth, pnDir, pnFile);
}

int fs_list_fini_callback(FSListEntry *pEnt, void *argp){

	if(0 != pEnt->nChild){
		printf("%s:L%d: ", __FUNCTION__, __LINE__);
		printf("0 != pEnt->nChild (%p)\n", pEnt->child);
		printf("path_full: %s\n", pEnt->path_full);
		// sceKernelDelayThread(10000);
		// *(int *)(~0xFFFF) = 0xBF00BF00;
	}

	if(NULL != pEnt->parent){
		pEnt->parent->nChild -= 1;
	}

	fs_list_free_entry(pEnt);
	return 0;
}

int fs_list_fini(FSListEntry *pEnt){
	return fs_list_execute2(pEnt, fs_list_fini_callback, NULL);
}

int fs_list_execute(FSListEntry *pEnt, int (* callback)(FSListEntry *ent, void *argp), void *argp){

	int res;
	FSListEntry *curr = pEnt;

	if(curr == NULL){
		return -1;
	}

	while(curr != NULL){

		res = callback(curr, argp);
		if(res < 0){
			return res;
		}

		if(curr->nChild != 0){
			curr = curr->child;
		}else if(curr->next != NULL){
			curr = curr->next;
		}else{
			while(curr != NULL && curr->next == NULL){
				curr = curr->parent;
			}
			if(curr != NULL){
				curr = curr->next;
			}
		}
	}

	return 0;
}

int fs_list_execute2(FSListEntry *pEnt, int (* callback)(FSListEntry *ent, void *argp), void *argp){

	int res;
	FSListEntry *curr = pEnt, *tmp;

	while(curr != NULL){
		if(curr->nChild != 0){
			curr = curr->child;
		}else if(curr->next != NULL){
			tmp = curr->next;

			res = callback(curr, argp);
			if(res < 0){
				return res;
			}

			curr = tmp;
		}else{
			while(curr != NULL && curr->next == NULL){
				tmp = curr->parent;

				res = callback(curr, argp);
				if(res < 0){
					return res;
				}

				curr = tmp;
			}
			if(curr != NULL){
				tmp = curr->next;

				res = callback(curr, argp);
				if(res < 0){
					return res;
				}

				curr = tmp;
			}
		}
	}

	return 0;
}

FSListEntry *fs_list_search_entry_by_name(FSListEntry *pEnt, const char *name){

	FSListEntry *curr = pEnt;

	while(NULL != curr){
		if(0 == strcmp(curr->name, name)){
			return curr;
		}

		curr = curr->next;
	}

	return NULL;
}

FSListEntry *fs_list_search_entry_by_path(FSListEntry *pEnt, const char *path){

	int len, last_block;
	FSListEntry *curr, *result;
	const char *curr_path, *curr_end;
	char *temp_name;

	if(NULL == pEnt || NULL == path){
		return NULL;
	}

	last_block = 0;
	curr = pEnt->child;
	curr_path = path;
	curr_end  = NULL;

	while(NULL != curr){
		if(curr_end != curr_path){
			curr_end = strchr(curr_path, '/');
			if(NULL == curr_end){
				int n = strlen(curr_path);
				curr_end = &curr_path[n];
				last_block = 1;
			}

			len = curr_end - curr_path;

			temp_name = malloc(len + 1);
			if(NULL == temp_name){
				break;
			}

			temp_name[len] = 0;
			memcpy(temp_name, curr_path, len);

			curr_path = curr_end;
		}

		result = fs_list_search_entry_by_name(curr, temp_name);

		if(NULL != result){
			free(temp_name);
			temp_name = NULL;

			if(0 != last_block){
				return result;
			}
			curr_path = &curr_end[1];
			curr = result->child;
		}else{
			curr = curr->next;
		}
	}

	free(temp_name);
	temp_name = NULL;

	return NULL;
}
