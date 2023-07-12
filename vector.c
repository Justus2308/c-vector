#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"


// Internal structs, variables, macros and functions

struct vinternal_Vec
{
	void *data;

	size_t elem_size;
	size_t len, cap;

	void *first, *last;
	size_t offset;

	int error;

	uint8_t status;
	uint8_t config;
};

struct vinternal_VecIter
{
	Vec *vec;

	size_t pos;
	void *finger;
};


enum
{
	VINTERNAL_STAT_ERROR = 1 << 0,
	VINTERNAL_STAT_OVERFLOW = 1 << 1,
	VINTERNAL_STAT_UNDERFLOW = 1 << 2,
	VINTERNAL_STAT_OWNED = 1 << 3,
};


static const size_t VINTERNAL_SIZE_T_MAX = ~((size_t) 0);
static const size_t VINTERNAL_HALF_SIZE_T_MAX = VINTERNAL_SIZE_T_MAX >> 1;


static uint8_t vinternal_base_cfg = 0;

static size_t vinternal_base_cap = VEC_DEFAULT_BASE_CAP;


#define vinternal_s_error(vec)				(vec->status & VINTERNAL_STAT_ERROR)
#define vinternal_s_overflow(vec)			(vec->status & VINTERNAL_STAT_OVERFLOW)
#define vinternal_s_underflow(vec)			(vec->status & VINTERNAL_STAT_UNDERFLOW)
#define vinternal_s_owned(vec)				(vec->status & VINTERNAL_STAT_OWNED)

#define vinternal_s_set(vec, stat)			vec->status |= stat
#define vinternal_s_unset(vec, stat)		vec->status &= ~stat

#define vinternal_c_noautogrow(vec)			(vec->config & V_NOAUTOGROW)
#define vinternal_c_noautoshrink(vec)		(vec->config & V_NOAUTOSHRINK)
#define vinternal_c_carryerror(vec)			(vec->config & V_CARRYERROR)
#define vinternal_c_allowoutofbounds(vec)	(vec->config & V_ALLOWOUTOFBOUNDS)
#define vinternal_c_iternocopy(vec)			(vec->config & V_ITERNOCOPY)
#define vinternal_c_exactsizing(vec)		(vec->config & V_EXACTSIZING)
#define vinternal_c_rawnocopy(vec)			(vec->config & V_RAWNOCOPY)
#define vinternal_c_keepoffset(vec)			(vec->config & V_KEEPOFFSET)


static inline size_t vinternal_double_cap(size_t n)
{
	if (n == 0)
	{
		return 1;
	}

	if (n > VINTERNAL_HALF_SIZE_T_MAX)
	{
		return VINTERNAL_SIZE_T_MAX;
	}

	return n << 1;
}

static inline size_t vinternal_add_to_cap(size_t x, size_t y)
{
	if (x > VINTERNAL_SIZE_T_MAX - y)
	{
		return VINTERNAL_SIZE_T_MAX;
	}

	return x + y;
}

static inline size_t vinternal_sub_from_cap(size_t x, size_t y)
{
	if (y > x)
	{
		return 0;
	}

	return x - y;
}


// Implementations functions exposed to user

void vc_set_base_cfg(enum VecCfg config)
{
	vinternal_base_cfg = config;
}

void vc_set_base_cap(size_t base_cap)
{
	vinternal_base_cap = base_cap;
}


Vec *v_create_with(size_t elem_size, size_t cap)
{
	Vec *vec = malloc(sizeof(Vec));
	if (vec == NULL) return NULL;

	vec->data = malloc(elem_size * cap);
	if (vec->data == NULL && cap != 0)
	{
		free(vec);
		return NULL;
	}

	vec->elem_size = elem_size;

	vec->len = 0;
	vec->cap = cap;

	vec->first = vec->data;
	vec->last = vec->first;
	vec->offset = 0;

	vec->status = 0;
	vec->config = vinternal_base_cfg;

	return vec;
}

Vec *v_create(size_t elem_size)
{
	return v_create_with(elem_size, vinternal_base_cap);
}


void v_set_cfg(Vec *vec, enum VecCfg config)
{
	vec->config = config;
}

void v_add_cfg(Vec *vec, enum VecCfg config)
{
	vec->config |= config;
}

void v_remove_cfg(Vec *vec, enum VecCfg config)
{
	vec->config &= ~config;
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

	void *new_data = realloc(vec->data, vec->elem_size * (size + vec->offset));
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

int v_reduce(Vec *vec)
{
	return v_set_size(vec, vec->len);
}

int v_grow(Vec *vec, size_t by_size)
{
	size_t new_cap = vinternal_add_to_cap(vec->cap, by_size);
	return v_set_size(vec, new_cap);
}

int v_shrink(Vec *vec, size_t by_size)
{
	size_t new_cap = vinternal_sub_from_cap(vec->cap, by_size);
	return v_set_size(vec, new_cap);
}


int v_push(Vec *vec, void *elem)
{
	if (vec->len == vec->cap)
	{
		if (vinternal_c_noautogrow(vec)) return 1;

		size_t new_size = vinternal_double_cap(vec->cap);

		if (v_set_size(vec, new_size))
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

void *v_pop_ptr(Vec *vec)
{
	if (vec->len == 0)
	{
		vec->error = 1;
		return NULL;
	}

	void *old_elem = vec->last;

	vec->last = ((char*)vec->last) - vec->elem_size;
	vec->len--;

	size_t half_cap = vec->cap >> 1;

	if (vec->len <= half_cap && !vinternal_c_noautoshrink(vec))
	{
		v_set_size(vec, half_cap);
	}

	return old_elem;
}


void *v_first_ptr(Vec *vec)
{
	return vec->first;
}

void *v_last_ptr(Vec *vec)
{
	return vec->last;
}


void *v_at_ptr(Vec *vec, size_t index)
{
	if (index > vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec)) return NULL;
		return vec->last;
	}

	return ((char*)vec->first) + (vec->elem_size * index);
}

int v_insert(Vec *vec, size_t index, void *elem)
{
	if (index >= vec->cap)
	{
		if (!vinternal_c_allowoutofbounds(vec))
		{
			return v_push(vec, elem);
		}

		if (vinternal_c_noautogrow(vec)) return 1;

		if (v_set_size(vec, vec->cap << 1))
		{
			return 1;
		}

		return v_insert(vec, index, elem);
	}

	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
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

void *v_remove_ptr(Vec *vec, size_t index)
{
	if (index >= vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
		{
			return NULL;
		}

		return v_pop_ptr(vec);
	}

	void *removed = malloc(vec->elem_size);

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

	if (vec->len <= half_cap && !vinternal_c_noautoshrink(vec))
	{
		v_set_size(vec, half_cap);
	}

	return removed;
}


int v_swap_insert(Vec *vec, size_t index, void *elem)
{
	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
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

void *v_swap_remove_ptr(Vec *vec, size_t index)
{
	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
		{
			return v_pop_ptr(vec);
		}
		return NULL;
	}

	void *swap = v_pop_ptr(vec);
	if (swap == NULL) return NULL;

	void *removed = malloc(vec->elem_size);

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
	if (vinternal_c_rawnocopy(vec))
	{
		return vec->first;
	}

	size_t raw_size = vec->len * vec->elem_size;

	void *raw = malloc(raw_size);

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

	if (vinternal_c_rawnocopy(vec))
	{
		return ((char*)vec->data) + (from * vec->elem_size);
	}

	size_t raw_slice_size = (to - from) * vec->elem_size;

	if (raw_slice_size > vec->len)
	{
		return v_raw(vec);
	}

	void *raw_slice = malloc(raw_slice_size);

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

	slice->elem_size = vec->elem_size;
	slice->len = slice_size;
	slice->first = slice->data;
	slice->last = ((char*)slice->data) + (to * vec->elem_size);

	slice->status = vec->status;
	slice->config = vec->config;

	return slice;
}


int v_prepend(Vec *vec, void *src, size_t len)
{
	// check if offset is big enough to just move first ptr and copy data to it
	// if not: check if cap is big enough
	// yes -> memmove, memcpy
	// no -> double cap, memmove, memcpy

	if (vec->offset >= len)
	{
		vec->offset -= len;
		vec->first = memcpy(
			((char*)vec->data) + (vec->offset * vec->elem_size),
			src,
			len * vec->elem_size);

		return 0;
	}

	size_t new_len = vec->len + len;
	size_t min_cap = new_len - vec->offset;

	if (min_cap > vec->cap && vinternal_c_noautogrow(vec))
	{
		return 1;
	}
	// WARNING: if offset is changed in v_set_size min_cap needs to be recalculated after every iteration!
	while (min_cap > vec->cap)
	{
		if (v_set_size(vec, vec->cap << 1))
		{
			return 1;
		}
	}

	memmove(
		((char*)vec->data) + (len * vec->elem_size),
		vec->first,
		vec->len);

	vec->first = memcpy(
		vec->data,
		src,
		len);

	vec->offset = 0;
	vec->len += len;

	vec->last = ((char*)vec->data) + (vec->len * vec->elem_size);

	return 0;
}

int v_append(Vec *vec, void *src, size_t len)
{

}


Vec *v_clone(Vec *vec)
{
	Vec *clone = malloc(sizeof(Vec));
	if (clone == NULL) return NULL;

	clone->data = malloc(vec->cap * vec->elem_size);
	if (clone->data == NULL)
	{
		free(clone);
		return NULL;
	}

	memcpy(
		clone->data,
		vec->data,
		vec->len);

	clone->elem_size = vec->elem_size;
	clone->len = vec->len;
	clone->first = ((char*)clone->data) + (vec->offset * clone->elem_size);
	clone->last = ((char*)clone->data) + (vec->len * clone->elem_size);

	clone->status = vec->status;
	clone->config = vec->config;

	return clone;
}

void v_zero(Vec *vec)
{
	memset(vec->data, 0, (vec->len * vec->elem_size));
}

void v_softclear(Vec *vec)
{
	vec->len = 0;
}

void v_clear(Vec *vec)
{
	free(vec->data);

	vec->data = malloc(0);

	vec->len = 0;
	vec->cap = 0;

	vec->first = vec->data;
	vec->last = vec->first;

	vec->offset = 0;
}

void v_destroy(Vec *vec)
{
	free(vec->data);
	free(vec);
}


VecIter *v_iter(Vec *vec)
{
	VecIter *iter = malloc(sizeof(VecIter));
	if (iter == NULL) return NULL;

	iter->pos = 0;

	if (vec->len == 0)
	{
		iter->vec = NULL;
		iter->finger = NULL;

		return iter;
	}

	if (vinternal_c_iternocopy(vec))
	{
		iter->vec = vec;
		vinternal_s_set(iter->vec, VINTERNAL_STAT_OWNED);
		iter->finger = vec->first;

		return iter;
	}

	iter->vec = v_slice(vec, 0, vec->len - 1);
	if (iter->vec == NULL) return NULL;

	return iter;
}

VecIter *v_into_iter(Vec **restrict vec)
{
	VecIter *iter = malloc(sizeof(VecIter));

	iter->pos = 0;

	if ((*vec)->len == 0)
	{
		iter->vec = NULL;
		iter->finger = NULL;

		return iter;
	}

	iter->vec = *vec;
	*vec = NULL;
	vinternal_s_set(iter->vec, VINTERNAL_STAT_OWNED);
	iter->finger = iter->vec->first;

	return iter;
}


void vi_destroy(VecIter *iter)
{
	if (iter->vec != NULL && vinternal_s_owned(iter->vec))
	{
		free(iter->vec->data);
		free(iter->vec);
	}

	free(iter);
}


bool vi_is_owner(VecIter *iter)
{
	return vinternal_s_owned(iter->vec);
}

size_t vi_pos(VecIter *iter)
{
	return iter->pos;
}


void *vi_next_ptr(VecIter *iter)
{
	void *next = iter->finger;

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
	if (iter->vec == NULL) return NULL;

	if (vinternal_s_owned(iter->vec))
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


#undef vinternal_s_error
#undef vinternal_s_overflow
#undef vinternal_s_underflow
#undef vinternal_s_owned

#undef vinternal_s_set
#undef vinternal_s_unset

#undef vinternal_c_noautogrow
#undef vinternal_c_noautoshrink
#undef vinternal_c_carryerror
#undef vinternal_c_allowoutofbounds
#undef vinternal_c_iternocopy
#undef vinternal_c_exactsizing
#undef vinternal_c_rawnocopy
#undef vinternal_c_keepoffset
