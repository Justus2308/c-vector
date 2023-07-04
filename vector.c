#include <stdbool.h>
#include <stddef.h>
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
	bool owner;

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
	if (vec->data == NULL && cap != 0)
	{
		free(vec);
		return NULL;
	}

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
		v_set_size(vec, half_cap);
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
	if (index >= vec->cap)
	{
		if (!(vec->cfg & VALLOWOUTOFBOUNDS))
		{
			return v_push(vec, elem);
		}

		if (vec->cfg & VNOAUTOGROW) return 1;

		if (v_set_size(vec, vec->cap << 1))
		{
			return 1;
		}

		return v_insert(vec, index, elem);
	}

	if (index >= vec->len)
	{
		if (!(vec->cfg & VALLOWOUTOFBOUNDS))
		{
			return v_push(vec, elem);
		}

		vec->last = memcpy(
		((char*)vec->data) + (index * vec->elem_size),
		elem,
		vec->elem_size);

		memset(
			((char*)vec->last) + vec->elem_size,
			0,
			(index - vec->len) * vec->elem_size);

		vec->len = index + 1;

		return 0;
	}

	if (vec->len + 1 > vec->cap)
	{
		if (v_set_size(vec, vec->cap << 1))
		{
			return 1;
		}
	}

	memmove(
		((char*)vec->data) + ((index + 1) * vec->elem_size),
		((char*)vec->data) + (index * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	memcpy(
		((char*)vec->data) + (index * vec->elem_size),
		elem,
		vec->elem_size);

	vec->len++;

	return 0;
}

void *v_remove(Vec *vec, size_t index)
{
	if (index >= vec->len)
	{
		if (vec->cfg & VALLOWOUTOFBOUNDS)
		{
			return NULL;
		}

		return v_pop(vec);
	}

	void *removed = v_malloc(vec->elem_size);

	memcpy(
		removed,
		((char*)vec->data) + (index * vec->elem_size),
		vec->elem_size);

	memmove(
		((char*)vec->data) + (index * vec->elem_size),
		((char*)vec->data) + ((index + 1) * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	vec->len--;

	size_t half_cap = vec->cap >> 1;

	if (vec->len <= half_cap && !(vec->cfg & VNOAUTOSHRINK))
	{
		v_set_size(vec, half_cap);
	}

	return removed;
}

void v_zero(Vec *vec)
{
	memset(vec->data, 0, (vec->len * vec->elem_size));
}

void v_clear(Vec *vec)
{
	if (!(vec->cfg & VSOFTCLEAR))
	{
		v_free(vec->data);
		vec->data = NULL;
		vec->cap = 0;
		vec->first = NULL;
		vec->last = NULL;
	}

	vec->len = 0;
}

void v_destroy(Vec *vec)
{
	free(vec->data);
	free(vec);
}


VecIter *v_iter(Vec *vec)
{
	VecIter *iter = v_malloc(sizeof(VecIter));
	if (iter == NULL) return NULL;

	iter->pos = 0;

	if (vec->len == 0)
	{
		iter->vec = NULL;
		iter->owner = false;
		iter->finger = NULL;

		return iter;
	}

	if (vec->cfg & VITERNOCOPY)
	{
		iter->vec = vec;
		iter->owner = false;
		iter->finger = vec->first;

		return iter;
	}

	iter->vec = v_malloc(sizeof(Vec));
	if (iter->vec == NULL)
	{
		free(iter);
		return NULL;
	}

	iter->vec->data = v_malloc(vec->cap * vec->elem_size);
	if (iter->vec->data == NULL)
	{
		free(iter->vec);
		free(iter);
		return NULL;
	}

	iter->owner = true;

	iter->vec->cfg = vec->cfg;
	iter->vec->elem_size = vec->elem_size;

	iter->vec->len = vec->len;
	iter->vec->cap = vec->len;

	iter->vec->first = memcpy(
		iter->vec->data,
		vec->data,
		vec->len * vec->elem_size);
	iter->vec->last =
		((char*)iter->vec->first)
		+ (iter->vec->len * iter->vec->elem_size);

	return iter;
}

VecIter *v_into_iter(Vec **restrict vec)
{
	VecIter *iter = v_malloc(sizeof(VecIter));

	iter->pos = 0;

	if ((*vec)->len == 0)
	{
		iter->vec = NULL;
		iter->owner = false;
		iter->finger = NULL;

		return iter;
	}

	iter->vec = *vec;
	*vec = NULL;
	iter->owner = true;
	iter->finger = iter->vec->first;

	return iter;
}


void vi_destroy(VecIter *iter)
{
	if (iter->owner)
	{
		free(iter->vec->data);
		free(iter->vec);
	}

	free(iter);
}


size_t vi_pos(VecIter *iter)
{
	return iter->pos;
}


void *vi_next(VecIter *iter)
{
	void *next = iter->finger;

	if (next == NULL && (iter->vec->cfg & VITERSELFDESTRUCT))
	{
		vi_destroy(iter);
		return NULL;
	}

	iter->pos++;

	if (iter->pos >= iter->vec->len)
	{
		iter->pos = iter->vec->len;
		iter->finger = NULL;
	} else {
		iter->finger = ((char*)iter->finger) + iter->vec->elem_size;
	}

	return next;
}

void vi_skip(VecIter *iter, size_t amount)
{
	iter->pos += amount;

	if (iter->pos >= iter->vec->len)
	{
		iter->pos = iter->vec->len;
		iter->finger = NULL;
	} else {
		iter->finger = ((char*)iter->finger) + (iter->vec->elem_size * amount);
	}
}

void vi_goto(VecIter *iter, size_t index)
{
	iter->pos = index;

	if (iter->pos >= iter->vec->len)
	{
		iter->pos = iter->vec->len;
		iter->finger = NULL;
	} else {
		iter->finger = ((char*)iter->vec->first) + (iter->vec->elem_size * index);
	}
}


Vec *vi_from_iter(VecIter *iter)
{
	if (iter->owner)
	{
		Vec *vec = iter->vec;

		free(iter);

		return vec;
	}

	Vec *vec = v_malloc(sizeof(Vec));
	if (vec == NULL)
	{
		return NULL;
	}

	vec->data = v_malloc(iter->vec->cap * iter->vec->elem_size);
	if (iter->vec->data == NULL)
	{
		free(vec);
		return NULL;
	}

	vec->cfg = iter->vec->cfg;
	vec->elem_size = iter->vec->elem_size;

	vec->len = iter->vec->len;
	vec->cap = iter->vec->len;

	vec->first = memcpy(
		vec->data,
		iter->vec->data,
		vec->len * vec->elem_size);
	vec->last =
		((char*)vec->first)
		+ (vec->len * vec->elem_size);

	vi_destroy(iter);
	return vec;
}
