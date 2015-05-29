#include <stdlib.h>
#include <string.h>

#include "varray.h"

varray *varray_new(int element_size, int initial_allocation)
{
	varray *va;

	va = calloc(1, sizeof(varray));
	if (va == NULL)
		return NULL;

	if (varray_init(va, element_size, initial_allocation) == NULL) {
		free(va);
		return NULL;
	}

	return va;
}

varray *varray_init(varray *va, int element_size, int initial_allocation)
{
	va->element_size = element_size;
	va->allocation = initial_allocation;

	if (initial_allocation) {
		va->data = calloc(initial_allocation + 1, element_size);
		if (va->data == NULL) {
			return NULL;
		}
	}

	return va;
}

void varray_destroy(varray *va)
{
	int i;

	if (va->destroy_func != NULL) {
		for (i = 0; i < va->count; i++) {
			va->destroy_func(va->data + i);
		}
	}
	free(va->data);
	memset(va, 0, sizeof(varray));
}

void varray_free(varray *va)
{
	if (va == NULL)
		return;

	varray_destroy(va);
	free(va);
}

int varray_get_index(varray *va, void *element_ptr)
{
	if (va->data == NULL)
		return -1;

	if (element_ptr < va->data || element_ptr >= va->data + (va->count * va->element_size))
		return -1;

	if ((element_ptr - va->data) % va->element_size != 0)
		return -1;

	return (element_ptr - va->data) / va->element_size;
}

static int grow_array(varray *va)
{
	int new_allocation;
	void *new_data;

	if (va->allocation == 0)
		new_allocation = 16;
	else
		new_allocation = va->allocation * 2;

	new_data = realloc(va->data, va->element_size * (new_allocation + 1));
	if (new_data == NULL)
		return 0;

	va->data = new_data;
	va->allocation = new_allocation;
	return 1;
}
#define GROW_IF_NECESSARY(va, failure_retval) do { \
	if ((va)->count >= (va)->allocation) { \
		if (!grow_array(va)) \
			return (failure_retval); \
	} \
} while (0)

void *varray_push(varray *va, void *element)
{
	GROW_IF_NECESSARY(va, NULL);
	if (element == NULL) {
		memset(va->data + va->count, 0, va->element_size);
		if (va->init_func) {
			if (va->init_func(va->data + va->count) == NULL)
				return NULL;
		}
	} else {
		memcpy(va->data + va->count, element, va->element_size);
	}
	va->count++;

	return va->data + va->count - 1;
}

void *varray_insert(varray *va, void *element, int index)
{
	if (index > va->count || index < 0)
		return NULL;

	GROW_IF_NECESSARY(va, NULL);
	if (index < va->count) {
		memmove(va->data + index + 1, va->data + index, va->element_size * (va->count - index));
	}

	if (element == NULL) {
		memset(va->data + index, 0, va->element_size);
		if (va->init_func) {
			if (va->init_func(va->data + index) == NULL) {
				varray_remove(va, index);
				return NULL;
			}
		}
	} else {
		memcpy(va->data + index, element, va->element_size);
	}

	va->count++;
	return va->data + index;
}

void *varray_pop(varray *va)
{
	if (va->count == 0)
		return NULL;

	va->count--;
	return va->data + va->count;
}

void *varray_remove(varray *va, int index)
{
	if (index >= va->count || index < 0)
		return NULL;

	if (index == va->count - 1)
		return varray_pop(va);

	memcpy(va->data + va->allocation, va->data + index, va->element_size);
	memmove(va->data + index, va->data + index + 1, va->element_size * (va->count - index));
	va->count--;

	memcpy(va->data + va->count, va->data + va->allocation, va->element_size);
	return va->data + va->count;
}

void varray_sort(varray *va)
{
	qsort(va->data, va->count, va->element_size, va->sort_compar);
}


void *varray_sorted_search(const varray *va, const void *key)
{
	return bsearch(key, va->data, va->count, va->element_size, va->search_compar ? va->search_compar : va->sort_compar);
}

static int get_sorted_insert_index(const varray *va, const void *key_or_element, int (*compar)(const void *, const void *))
{
	int low_idx = 0, high_idx = va->count;

	while (low_idx < high_idx) {
		int test_idx = (low_idx + high_idx) / 2;
		int result = compar(key_or_element, va->data + test_idx);

		if (result == 0)
			return test_idx;
		else if (result > 0)
			low_idx = test_idx + 1;
		else
			high_idx = test_idx;
	}

	return low_idx;
}

void *varray_sorted_insert_ex(varray *va, void *element, int allow_dup)
{
	int insert_idx = get_sorted_insert_index(va, element, va->sort_compar);

	if (!allow_dup && insert_idx < va->count && va->search_compar(element, va->data + insert_idx) == 0)
		return NULL;

	return varray_insert(va, element, insert_idx);
}

void *varray_sorted_insert(varray *va, void *element)
{
	return varray_sorted_insert_ex(va, element, 1);
}

void *varray_sorted_search_or_insert(varray *va, const void *key, int *found_existing)
{
	int (*compar)(const void *, const void *);
	int insert_idx;

	compar = va->search_compar ? va->search_compar : va->sort_compar;

	insert_idx = get_sorted_insert_index(va, key, compar);

	if (insert_idx < va->count && compar(key, va->data + insert_idx) == 0) {
		if (found_existing != NULL)
			*found_existing = 1;
		return va->data + insert_idx;
	} else {
		if (found_existing != NULL)
			*found_existing = 0;
		return varray_insert(va, NULL, insert_idx);
	}
}
