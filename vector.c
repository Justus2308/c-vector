#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vector.h"


// Internal structs, variables, macros and functions

struct vinternal_Vec
{
	void *data;

	size_t elem_size;
	size_t len, cap;

	void *first, *last;
	size_t offset;

	uint8_t config;
};

struct vinternal_VecIter
{
	Vec *vec;

	size_t pos;
	void *finger;

	bool owner;
	bool done;
};


static const char *const VINTERNAL_ERROR_STRINGS[] =
{
	[VE_OK]				= "No error has occured.",
	[VE_NOMEM]			= "Cannot allocate memory.",
	[VE_EMPTY]			= "Vector is empty.",
	[VE_OUTOFBOUNDS]	= "Tried to access index out of bounds of vector.",
	[VE_TOOLONG]		= "Vector doesn't fit into the requested capacity.",
	[VE_NOCAP]			= "Vector doesn't have enough capacity left.",
	[VE_NODEST]			= "Destination pointer is invalid.",
};

static const size_t VINTERNAL_HALF_SIZE_MAX = SIZE_MAX >> 1;


static uint8_t vinternal_base_cfg = VEC_DEFAULT_BASE_CFG;

static size_t vinternal_base_cap = VEC_DEFAULT_BASE_CAP;


static FILE *vinternal_out_stream = NULL;
static FILE *vinternal_err_stream = NULL;

static bool vinternal_verbose = false;


#define vinternal_c_noautogrow(vec)			(vec->config & V_NOAUTOGROW)
#define vinternal_c_noautoshrink(vec)		(vec->config & V_NOAUTOSHRINK)
#define vinternal_c_allowoutofbounds(vec)	(vec->config & V_ALLOWOUTOFBOUNDS)
#define vinternal_c_iternocopy(vec)			(vec->config & V_ITERNOCOPY)
#define vinternal_c_exactsizing(vec)		(vec->config & V_EXACTSIZING)
#define vinternal_c_rawnocopy(vec)			(vec->config & V_RAWNOCOPY)
#define vinternal_c_keepoffset(vec)			(vec->config & V_KEEPOFFSET)


#define vinternal_return_maybe(func)										\
do {																		\
	int vinternal_macro_tmp_retval__ = func;								\
	if (vinternal_macro_tmp_retval__)										\
		return vinternal_macro_tmp_retval__;								\
} while(0)


static inline
size_t vinternal_double_cap(size_t n)
{
	if (n > VINTERNAL_HALF_SIZE_MAX)
	{
		return (size_t) SIZE_MAX;
	}

	if (n == 0)
	{
		return 1;
	}

	return n << 1;
}

static inline
size_t vinternal_size_t_add(size_t x, size_t y)
{
	if (x > ((size_t) SIZE_MAX) - y)
	{
		return (size_t) SIZE_MAX;
	}

	return x + y;
}

static inline
size_t vinternal_size_t_sub(size_t x, size_t y)
{
	if (y > x)
	{
		return 0;
	}

	return x - y;
}

static inline
size_t vinternal_real_cap(Vec *vec)
{
	if (vinternal_c_keepoffset(vec))
		return vec->cap - vec->offset;

	return vec->cap;
}

static inline
void vinternal_zero_offset_maybe(Vec *vec, size_t min_cap)
{
	if (vec->cap - vec->offset < min_cap)
	{
		memmove(
			vec->data,
			vec->first,
			vec->len * vec->elem_size);

		vec->offset = 0;
	}
}

static
void vinternal_log_to_stream(FILE *stream, char *tag, char *msg)
{
	time_t current_time_raw = time(NULL);
	struct tm *current_time = localtime(&current_time_raw);
	fprintf(stream, "[%d-%d-%d %d:%d:%d] [%s] : %s\n",
		current_time->tm_year,
		current_time->tm_mon,
		current_time->tm_mday,
		current_time->tm_hour,
		current_time->tm_min,
		current_time->tm_sec,
		tag,
		msg);
}

static inline
void vinternal_log(char *tag, char *msg)
{
	vinternal_log_to_stream(vinternal_out_stream ? vinternal_out_stream : stdout, tag, msg);
}

static inline
void vinternal_err(char *tag, char *msg)
{
	vinternal_log_to_stream(vinternal_err_stream ? vinternal_err_stream : stderr, tag, msg);
}

static inline
void vinternal_vlog(char *tag, char *msg)
{
	if (vinternal_verbose)
		vinternal_log(tag, msg);
}

static inline
void vinternal_verr(char *tag, char *msg)
{
	if (vinternal_verbose)
		vinternal_err(tag, msg);
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

void vc_set_output_stream(FILE *stream)
{
	vinternal_out_stream = stream;
}

void vc_set_error_stream(FILE *stream)
{
	vinternal_err_stream = stream;
}

void vc_set_verbose(bool verbose)
{
	vinternal_verbose = verbose;
}


void v_perror(const char *str, enum VecErr err)
{
	const char *colon;

	if (str == NULL || *str == '\0')
		str = colon = "";
	else
		colon = ": ";

	if (err < 0 || err >= VINTERNAL_LAST)
		err = VE_OK;

	fprintf(
		vinternal_err_stream ? vinternal_err_stream : stderr,
		"%s%s%s\n",
		str, colon, VINTERNAL_ERROR_STRINGS[err]);
}


Vec *v_create_with(size_t elem_size, size_t cap)
{
	Vec *vec = malloc(sizeof(Vec));
	if (vec == NULL)
		return NULL;

	if (cap == 0)
	{
		vec->data = NULL;
	}
	else
	{
		vec->data = malloc(elem_size * cap);
		if (vec->data == NULL)
		{
			free(vec);
			return NULL;
		}
	}

	vec->elem_size = elem_size;

	vec->len = 0;
	vec->cap = cap;

	vec->first = vec->last = NULL;
	vec->offset = 0;

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


void v_clear(Vec *vec)
{
	free(vec->data);

	vec->data = vec->first = vec->last = NULL;
	vec->len = vec->cap = vec->offset = 0;
}


int v_set_size(Vec *vec, size_t size)
{
	if (size < vec->len)
		return VE_TOOLONG;

	if (size == 0)
	{
		v_clear(vec);
		return VE_OK;
	}

	if (vec->offset == 0 || vinternal_c_keepoffset(vec))
	{
		void *new_data = realloc(vec->data, (size + vec->offset) * vec->elem_size);

		if (new_data == NULL)
			return VE_NOMEM;

		vec->data = new_data;
	}
	else if (vec->offset >= size) // ???
	{
		memmove(vec->data, vec->first, vec->len);
		vec->offset = 0;
	}
	else
	{
		void *new_data = malloc(size * vec->elem_size);

		if (new_data == NULL)
			return VE_NOMEM;

		memcpy(
			new_data,
			((char *)vec->data) + (vec->offset * vec->elem_size),
			vec->len * vec->elem_size);

		free(vec->data);

		vec->data = new_data;
		vec->offset = 0;
	}

	if (vec->len != 0)
	{
		vec->first = ((char*)vec->data) + (vec->offset * vec->elem_size);
		vec->last = ((char*)vec->first) + (vec->len * vec->elem_size);
	}

	vec->cap = size;

	return VE_OK;
}

int v_reduce(Vec *vec)
{
	return v_set_size(vec, vec->len);
}

int v_reduce_strict(Vec *vec)
{
	if (vec->len == 0)
	{
		free(vec->data);

		vec->data = malloc(0);

		vec->first = vec->data;
		vec->last = vec->first;

		vec->cap = vec->offset = 0;

		return VE_OK;
	}

	if (vec->offset == 0)
	{
		void *reduced = realloc(vec->data, vec->len * vec->elem_size);
		if (reduced == NULL)
			return VE_NOMEM;

		vec->data = reduced;
		vec->cap = vec->len;

		return VE_OK;
	}

	void *reduced = malloc(vec->len * vec->elem_size);
	if (reduced == NULL)
		return VE_NOMEM;

	vec->first = memcpy(
		reduced,
		vec->first,
		vec->len * vec->elem_size);

	vec->last = ((char *)vec->first) + (vec->len * vec->elem_size);

	free(vec->data);
	vec->data = reduced;

	vec->cap = vec->len;
	vec->offset = 0;

	return VE_OK;
}

int v_grow(Vec *vec, size_t by_size)
{
	size_t new_cap = vinternal_size_t_add(vec->cap, by_size);
	return v_set_size(vec, new_cap);
}

int v_shrink(Vec *vec, size_t by_size)
{
	size_t new_cap = vinternal_size_t_sub(vec->cap, by_size);
	return v_set_size(vec, new_cap);
}


int v_push(Vec *vec, void *elem)
{
	if (elem == NULL)
		return VE_OK;

	if (vec->len == vec->cap)
	{
		if (vinternal_c_noautogrow(vec)) return VE_NOCAP;

		vinternal_return_maybe(v_set_size(vec, vinternal_double_cap(vec->cap)));
	}

	if (vec->len == 0)
	{
		vec->first = ((char*)vec->data) + vec->offset;
		vec->last = vec->first;
	}
	else
	{
		vec->last = ((char *)vec->last) + vec->elem_size;
	}

	vec->len++;

	memcpy(
		vec->last,
		elem,
		vec->elem_size);

	return VE_OK;
}

int v_pop(Vec *vec, void *dest)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	memcpy(
		dest,
		vec->last,
		vec->elem_size);

	vec->last = ((char*)vec->last) - vec->elem_size;
	vec->len--;

	size_t half_cap = vec->cap >> 1;

	if (vec->len <= half_cap && !vinternal_c_noautoshrink(vec))
	{
		vinternal_return_maybe(v_set_size(vec, half_cap));
	}

	return VE_OK;
}


int v_first(Vec *vec, void *dest)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	memcpy(
		dest,
		vec->first,
		vec->elem_size);

	return VE_OK;
}

int v_last(Vec *vec, void *dest)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	memcpy(
		dest,
		vec->last,
		vec->elem_size);

	return VE_OK;
}


int v_at(Vec *vec, void *dest, size_t index)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index > vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
			return VE_OUTOFBOUNDS;

		return v_last(vec, dest);
	}

	memcpy(
		dest,
		((char*)vec->first) + (vec->elem_size * index),
		vec->elem_size);

	return VE_OK;
}

int v_insert(Vec *vec, void *elem, size_t index)
{
	if (elem == NULL)
		return VE_OK;

	if (index >= vec->cap)
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_push(vec, elem);

		if (vinternal_c_noautogrow(vec))
			return VE_OUTOFBOUNDS;

		while (vec->cap < index)
		{
			vinternal_return_maybe(v_set_size(vec, vinternal_double_cap(vec->cap)));
		}
	}

	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_push(vec, elem);

		if (vec->len == 0)
		{
			vec->first = ((char *)vec->data) + vec->offset;
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

		return VE_OK;
	}

	size_t new_len = vec->len + 1;

	if (new_len > vec->cap - vec->offset)
	{
		if (vinternal_c_noautogrow(vec))
			return VE_NOCAP;

		vinternal_return_maybe(v_set_size(vec, vinternal_double_cap(vec->cap)));
	}

	memmove(
		((char*)vec->first) + ((index + 1) * vec->elem_size),
		((char*)vec->first) + (index * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	memcpy(
		((char*)vec->first) + (index * vec->elem_size),
		elem,
		vec->elem_size);

	vec->len = new_len;

	return VE_OK;
}

int v_remove(Vec *vec, void *dest, size_t index)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index >= vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
			return VE_OUTOFBOUNDS;

		return v_pop(vec, dest);
	}

	memcpy(
		dest,
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
		vinternal_return_maybe(v_set_size(vec, half_cap));
	}

	return VE_OK;
}


int v_swap_insert(Vec *vec, void *elem, size_t index)
{
	if (elem == NULL)
		return VE_OK;

	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_push(vec, elem);

		return v_insert(vec, elem, index);
	}

	vinternal_return_maybe(v_push(vec, ((char*)vec->first) + (index * vec->elem_size)));

	memcpy(
		((char*)vec->first) + (index * vec->elem_size),
		elem,
		vec->elem_size);

	return VE_OK;
}

int v_swap_remove(Vec *vec, void *dest, size_t index)
{
	if (dest == NULL)
		return VE_NODEST;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_pop(vec, dest);

		return VE_OUTOFBOUNDS;
	}

	void *removed = ((char*)vec->first) + (index * vec->elem_size);

	memcpy(
		dest,
		removed,
		vec->elem_size);

	vinternal_return_maybe(v_pop(vec, removed));

	return VE_OK;
}


void *v_raw(Vec *vec)
{
	if (vec->len == 0)
		return NULL;

	if (vinternal_c_rawnocopy(vec))
		return vec->first;

	size_t raw_size = vec->len * vec->elem_size;

	void *raw = malloc(raw_size);
	if (raw == NULL)
		return NULL;

	return memcpy(
		raw,
		vec->first,
		raw_size);
}

void *v_raw_slice(Vec *vec, size_t from, size_t to)
{
	if (vec->len == 0)
		return NULL;

	if (from >= to)
		return NULL;

	if (from >= vec->len)
		return NULL;

	if (to >= vec->len)
		to = vec->len - 1;


	if (vinternal_c_rawnocopy(vec))
		return ((char*)vec->first) + (from * vec->elem_size);

	size_t raw_slice_size = (to - from) * vec->elem_size;

	void *raw_slice = malloc(raw_slice_size);
	if (raw_slice == NULL)
		return NULL;

	return memcpy(
		raw_slice,
		((char*)vec->first) + (from * vec->elem_size),
		raw_slice_size);
}


Vec *v_slice(Vec *vec, size_t from, size_t to)
{
	if (vec->len == 0)
		return NULL;

	if (from >= to)
		return NULL;

	if (from >= vec->len)
		return NULL;

	if (to >= vec->len)
		to = vec->len - 1;


	size_t slice_size = (to - from) * vec->elem_size;

	Vec *slice = v_create_with(vec->elem_size, slice_size);

	memcpy(
		slice->data,
		((char*)vec->first) + (from * vec->elem_size),
		slice_size);

	slice->elem_size = vec->elem_size;
	slice->len = slice_size;
	slice->first = slice->data;
	slice->last = ((char*)slice->first) + (slice->len * vec->elem_size);

	slice->config = vec->config;

	return slice;
}

// TODO
int v_prepend(Vec *vec, void *src, size_t amount)
{
	size_t new_len = vec->len + amount;
	size_t min_cap = new_len - vec->offset;

	if (min_cap > vec->cap && vinternal_c_noautogrow(vec))
		return VE_NOCAP;

	while (min_cap > vec->cap)
	{
		vinternal_return_maybe(v_set_size(vec, vinternal_double_cap(vec->cap)));

		min_cap = new_len - vec->offset;
	}

	if (vec->offset >= amount)
	{
		vec->offset -= amount;
		vec->first = memcpy(
			((char*)vec->data) + (vec->offset * vec->elem_size),
			src,
			amount * vec->elem_size);

		return VE_OK;
	}

	memmove(
		((char*)vec->data) + (amount * vec->elem_size),
		vec->first,
		vec->len * vec->elem_size);

	vec->first = memcpy(
		vec->data,
		src,
		amount * vec->elem_size);

	vec->offset = 0;
	vec->len = new_len;

	vec->last = ((char*)vec->first) + (vec->len * vec->elem_size);

	return VE_OK;
}
/*
int v_append(Vec *vec, void *src, size_t amount)
{

}
*/

Vec *v_clone(Vec *vec)
{
	Vec *clone = malloc(sizeof(Vec));
	if (clone == NULL)
		return NULL;

	clone->data = malloc(vec->cap * vec->elem_size);
	if (clone->data == NULL)
	{
		free(clone);
		return NULL;
	}

	clone->elem_size = vec->elem_size;
	clone->len = vec->len;
	clone->cap = vec->cap;
	clone->offset = vec->offset;

	clone->first = memcpy(
		((char *)clone->data) + clone->offset,
		vec->first,
		vec->len * vec->elem_size);

	clone->last = ((char*)clone->first) + (clone->len * clone->elem_size);

	clone->config = vec->config;

	return clone;
}

void v_zero(Vec *vec)
{
	memset(vec->first, 0, (vec->len * vec->elem_size));
}

void v_softclear(Vec *vec)
{
	vec->len = 0;
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
		iter->owner = false;
		iter->done = true;

		return iter;
	}

	iter->done = false;

	if (vinternal_c_iternocopy(vec))
	{
		iter->vec = vec;
		iter->finger = vec->first;
		iter->owner = true;

		return iter;
	}

	iter->vec = v_slice(vec, 0, vec->len - 1);
	if (iter->vec == NULL) return NULL;

	iter->owner = false;

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
		iter->owner = false;
		iter->done = true;

		return iter;
	}

	iter->vec = *vec;
	*vec = NULL;
	iter->finger = iter->vec->first;
	iter->owner = true;
	iter->done = false;

	return iter;
}


void vi_destroy(VecIter *iter)
{
	if (iter->vec != NULL && iter->owner)
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

bool vi_done(VecIter *iter)
{
	return iter->done;
}


int vi_next(VecIter *iter, void *dest)
{
	if (iter->finger == NULL)
	{
		return 1;
	}

	memcpy(
		dest,
		iter->finger,
		iter->vec->elem_size);

	iter->pos++;

	if (iter->pos == iter->vec->len)
	{
		iter->pos = iter->vec->len;
		iter->finger = NULL;
	} else {
		iter->finger = ((char*)iter->finger) + iter->vec->elem_size;
	}

	return 0;
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


#undef vinternal_c_noautogrow
#undef vinternal_c_noautoshrink
#undef vinternal_c_carryerror
#undef vinternal_c_allowoutofbounds
#undef vinternal_c_iternocopy
#undef vinternal_c_exactsizing
#undef vinternal_c_rawnocopy
#undef vinternal_c_keepoffset

#undef vinternal_return_maybe
