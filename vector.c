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
	size_t offset;
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
	vec->offset = 0;

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


size_t v_elem_size(Vec *vec)
{
	return vec->elem_size;
}

size_t v_len(Vec *vec)
{
	return vec->len;
}

size_t v_cap(Vec *vec)
{
	return vec->cap;
}

// TODO: check offset, memmove vec if worth it
int v_set_size(Vec *vec, size_t size)
{
	if (size < vec->len) return 1;

	void *new_data = v_realloc(vec->data, vec->elem_size * (size + vec->offset));
	if (new_data == NULL) return 1;

	vec->data = new_data;

	if (vec->len != 0)
	{
		vec->first = ((char*)new_data) + vec->offset;
		vec->last = ((char*)vec->first) + (vec->len * vec->elem_size);
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
			vec->first = ((char*)vec->data) + vec->offset;
			vec->last = vec->first;
		}
	}

	vec->last = memcpy(
		((char*)vec->last) + 1,
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

	return ((char*)vec->first) + (vec->elem_size * index);
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
		((char*)vec->first) + (index * vec->elem_size),
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
		((char*)vec->first) + ((index + 1) * vec->elem_size),
		((char*)vec->first) + (index * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	memcpy(
		((char*)vec->first) + (index * vec->elem_size),
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
		((char*)vec->first) + (index * vec->elem_size),
		vec->elem_size);

	memmove(
		((char*)vec->first) + (index * vec->elem_size),
		((char*)vec->first) + ((index + 1) * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	vec->len--;

	size_t half_cap = vec->cap >> 1;

	if (vec->len <= half_cap && !(vec->cfg & VNOAUTOSHRINK))
	{
		v_set_size(vec, half_cap);
	}

	return removed;
}


int v_swap_insert(Vec *vec, size_t index, void *elem)
{
	if (index >= vec->len)
	{
		if (!(vec->cfg & VALLOWOUTOFBOUNDS))
		{
			return v_push(vec, elem);
		}
		return v_insert(vec, index, elem);
	}

	if (v_push(vec, ((char*)vec->first) + (index * vec->elem_size)))
	{
		return 1;
	}

	memcpy(
		((char*)vec->first) + (index * vec->elem_size),
		elem,
		vec->elem_size);

	return 0;
}

void *v_swap_remove(Vec *vec, size_t index)
{
	if (index >= vec->len)
	{
		if (!(vec->cfg & VALLOWOUTOFBOUNDS))
		{
			return v_pop(vec);
		}
		return NULL;
	}

	void *swap = v_pop(vec);
	if (swap == NULL) return NULL;

	void *removed = v_malloc(vec->elem_size);

	memcpy(
		removed,
		((char*)vec->first) + (index * vec->elem_size),
		vec->elem_size);

	memcpy(
		((char*)vec->first) + (index * vec->elem_size),
		swap,
		vec->elem_size);

	return removed;
}


void *v_raw(Vec *vec)
{
	if (vec->cfg & VRAWNOCOPY)
	{
		return vec->first;
	}

	size_t raw_size = vec->len * vec->elem_size;

	void *raw = v_malloc(raw_size);

	return memcpy(
		raw,
		vec->first,
		raw_size);
}

void *v_raw_slice(Vec *vec, size_t from, size_t to)
{
	if (from >= to) return NULL;

	if (from >= vec->len) return NULL;
	if (to >= vec->len) to = vec->len - 1;

	if (vec->cfg & VRAWNOCOPY)
	{
		return ((char*)vec->data) + (from * vec->elem_size);
	}

	size_t raw_slice_size = (to - from) * vec->elem_size;

	if (raw_slice_size > vec->len)
	{
		return v_raw(vec);
	}

	void *raw_slice = v_malloc(raw_slice_size);

	return memcpy(
		raw_slice,
		((char*)vec->first) + (from * vec->elem_size),
		raw_slice_size);
}


Vec *v_slice(Vec *vec, size_t from, size_t to)
{
	if (from >= to) return NULL;

	if (from >= vec->len) return NULL;
	if (to >= vec->len) to = vec->len - 1;

	size_t slice_size = (to - from) * vec->elem_size;

	Vec *slice = v_create_with(vec->elem_size, slice_size);

	memcpy(
		slice->data,
		((char*)vec->first) + (from * vec->elem_size),
		slice_size);

	slice->cfg = vec->cfg;
	slice->elem_size = vec->elem_size;
	slice->len = slice_size;
	slice->first = slice->data;
	slice->last = ((char*)slice->data) + (to * vec->elem_size);

	return slice;
}


Vec *v_clone(Vec *vec)
{
	Vec *clone = v_malloc(sizeof(Vec));
	if (clone == NULL) return NULL;

	clone->data = v_malloc(vec->cap * vec->elem_size);
	if (clone->data == NULL)
	{
		free(clone);
		return NULL;
	}

	memcpy(
		clone->data,
		vec->data,
		vec->len);

	clone->cfg = vec->cfg;
	clone->elem_size = vec->elem_size;
	clone->len = vec->len;
	clone->first = ((char*)clone->data) + (vec->offset * clone->elem_size);
	clone->last = ((char*)clone->data) + (vec->len * clone->elem_size);

	return clone;
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

	iter->vec = v_slice(vec, 0, vec->len - 1);
	if (iter->vec == NULL) return NULL;

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


bool vi_is_owner(VecIter *iter)
{
	return iter->owner;
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

	Vec *vec = v_clone(iter->vec);
	if (vec == NULL) return NULL;

	vi_destroy(iter);
	
	return vec;
}
