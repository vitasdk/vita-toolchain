#ifndef VARRAY_H
#define VARRAY_H

typedef struct {
	void *data;

	int count;
	int allocation; /* We actually allocate one more element than this for temp storage. */

	int element_size;

	/* Will be called when an empty element is created; returning NULL will fail the insert */
	void *(*init_func)(void *element);
	/* Will be called for each element on _destroy */
	void (*destroy_func)(void *element);

	/* Compare two array items, is passed to qsort */
	int (*sort_compar)(const void *el1, const void *el2);
	/* Compare a key and an array item, for search; sort_compar will be used if NULL */
	int (*search_compar)(const void *key, const void *element);
} varray;

varray *varray_new(int element_size, int initial_allocation); /* Allocate and initialize new varray */
varray *varray_init(varray *va, int element_size, int initial_allocation); /* Initialize new varray only */
void varray_destroy(varray *va); /* Deinitialize varray contents */
void varray_free(varray *va); /* Deinitialize and free memory */

void *varray_extract_array(varray *va); /* Compact array storage and return; va becomes uninitialized */

int varray_get_index(varray *va, void *element_ptr);

/* if element is NULL in either function, the new element will be initialized to zero. */
void *varray_push(varray *va, void *element);
void *varray_insert(varray *va, void *element, int index);

/* _pop and _remove will return a value that is only valid until the next array insert. */
void *varray_pop(varray *va);
void *varray_remove(varray *va, int index);

void varray_sort(varray *va);
void *varray_sorted_search(const varray *va, const void *key);
void *varray_sorted_insert(varray *va, void *element);
void *varray_sorted_insert_ex(varray *va, void *element, int allow_dup);
/* found_existing, if not NULL, will be set to 1 if an existing element was found, otherwise 0 */
void *varray_sorted_search_or_insert(varray *va, const void *key, int *found_existing);

#define VARRAY_ELEMENT(va, index) ((va)->data + ((index) * (va)->element_size))

#endif
