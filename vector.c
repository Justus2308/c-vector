#include <stdlib.h>
#include <string.h>

#include "vector.h"


struct vec
{
	unsigned char cfg;

	void *data;

	size_t elem_size;
	size_t len, cap;
	void *first, *last;
};

struct vec_iter
{
	Vec *vec;
	size_t pos;
	void *finger;
};


static unsigned char v_base_cfg = 0;

static size_t v_base_cap = 8;

static void *(*v_malloc)(size_t) = &malloc;
static void *(*v_calloc)(size_t, size_t) = &calloc;
static void *(*v_realloc)(void *, size_t) = &realloc;
static void (*v_free)(void *) = &free;


void vc_set_base_cfg(enum VecCfg config)
{
	v_base_cfg = config;
}

void vc_set_base_cap(size_t base_cap)
{
	v_base_cap = base_cap;
}

void vc_set_allocator(
	void *(*malloc_impl)(size_t),
	void *(*calloc_impl)(size_t, size_t),
	void *(*realloc_impl)(void *, size_t),
	void (*free_impl)(void *))
{
	v_malloc = malloc_impl;
	v_calloc = calloc_impl;
	v_realloc = realloc_impl;
	v_free = free_impl;
}


Vec *v_create_with(size_t elem_size, size_t cap)
{
	Vec *vec = v_malloc(sizeof(Vec));
	if (vec == NULL) return NULL;

	vec->cfg = v_base_cfg;

	vec->data = v_malloc(elem_size * cap);
	if (vec->data == NULL && cap != 0) return NULL;

	vec->elem_size = elem_size;

	vec->len = 0;
	vec->cap = cap;

	vec->first = NULL;
	vec->last = NULL;

	return vec;
}

Vec *v_create(size_t elem_size)
{
	return v_create_with(elem_size, v_base_cap);
}


void v_set_cfg(Vec *vec, enum VecCfg config)
{
	vec->cfg = (unsigned char)config;
}

void v_add_cfg(Vec *vec, enum VecCfg config)
{
	vec->cfg |= (unsigned char)config;
}

void v_remove_cfg(Vec *vec, enum VecCfg config)
{
	vec->cfg &= ~((unsigned char)config);
}


size_t v_len(Vec *vec)
{
	return vec->len;
}

size_t v_cap(Vec *vec)
{
	return vec->cap;
}


int v_set_size(Vec *vec, size_t size)
{
	if (size < vec->len) return 1;

	void *new_data = v_realloc(vec->data, vec->elem_size * size);
	if (new_data == NULL) return 1;

	vec->data = new_data;

	if (vec->len != 0)
	{
		vec->first = new_data;
		vec->last = ((char*)new_data) + (vec->len * vec->elem_size);
	}

	vec->cap = size;
	return 0;
}

int v_grow(Vec *vec, size_t by_size)
{
	return v_set_size(vec, vec->cap + by_size);
}

int v_shrink(Vec *vec, size_t by_size)
{
	return v_set_size(vec, vec->cap - by_size);
}


int v_push(Vec *vec, void *elem)
{
	if (vec->len == vec->cap)
	{
		if (vec->cfg & VNOAUTOGROW) return 1;

		if (v_set_size(vec, vec->cap << 1))
		{
			return 1;
		}

		if (vec->len == 0)
		{
			vec->first = vec->data;
		}
	}

	vec->last = memcpy(
		((char*)vec->data) + (vec->len * vec->elem_size),
		elem,
		vec->elem_size);

	vec->len++;
	return 0;
}

void *v_pop(Vec *vec)
{
	if (vec->first == NULL) return NULL;

	void *old_elem = vec->last;

	vec->last = ((char*)vec->last) - vec->elem_size;
	vec->len--;

	size_t half_cap = vec->cap >> 1;

	if (vec->len <= half_cap && !(vec->cfg & VNOAUTOSHRINK))
	{
		if (v_set_size(vec, half_cap))
		{
			vec->len++;
			return NULL;
		}
	}

	return old_elem;
}


void *v_first(Vec *vec)
{
	return vec->first;
}

void *v_last(Vec *vec)
{
	return vec->last;
}


void *v_at(Vec *vec, size_t index)
{
	if (index > vec->len)
	{
		if (vec->cfg & VALLOWOUTOFBOUNDS) return NULL;
		return vec->last;
	}

	return ((char*)vec->data) + (vec->elem_size * index);
}

int v_insert(Vec *vec, size_t index, void *elem)
{
	if (index == vec->len) return v_push(vec, elem);

	if (index > vec->len)
	{

		if (vec->cfg & VALLOWOUTOFBOUNDS)
		{
			
		}
	}

	return 0;
}
