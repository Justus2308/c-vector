#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vector.h"


const size_t VC_DEFAULT_BASE_CFG = 0;
const size_t VC_DEFAULT_BASE_CAP = 8;


// Internal structs, variables, macros and functions

/**
 * Internal vector struct.
 * Opaque to user.
 */
struct vinternal_Vec
{
	void *data;

	size_t elem_size;
	size_t len, cap;

	void *first, *last;
	size_t offset;

	uint8_t config;
};

/**
 * Internal iterator struct.
 * Opaque to user.
 */
struct vinternal_VecIter
{
	Vec *vec;

	size_t pos;
	void *finger;

	bool owner;
};

/**
 * Error strings for v_perror.
 */
static const char *const VINTERNAL_ERROR_STRINGS[] =
{
	[VE_OK]				= "No error has occured.",
	[VE_NOMEM]			= "Cannot allocate memory.",
	[VE_EMPTY]			= "Vector is empty.",
	[VE_OUTOFBOUNDS]	= "Tried to access index out of bounds of vector.",
	[VE_TOOLONG]		= "Vector doesn't fit into the requested capacity.",
	[VE_NOCAP]			= "Vector doesn't have enough capacity left.",
	[VE_INVAL]			= "Vector pointer points to invalid address.",
	[VE_NEEDSTDC11]		= "This function needs C11 or later to work.",
};

static const size_t VINTERNAL_HALF_SIZE_MAX = SIZE_MAX >> 1;


static uint8_t vinternal_base_cfg = VC_DEFAULT_BASE_CFG;

static size_t vinternal_base_cap = VC_DEFAULT_BASE_CAP;


static FILE *vinternal_out_stream = NULL;
static FILE *vinternal_err_stream = NULL;

static bool vinternal_verbose = false;


/**
 * Return from caller if the retval of func is non-zero.
 */
#define VMACRO_RETURN_MAYBE(func)											\
do {																		\
	const int VTMP_RETVAL__ = func;											\
	if (VTMP_RETVAL__)														\
		return VTMP_RETVAL__;												\
} while (false)


/**
 * Get information about the config of a vector.
 */
static inline
bool vinternal_c_noautogrow(Vec *vec)
{
	return (vec->config & V_NOAUTOGROW);
}
static inline
bool vinternal_c_noautoshrink(Vec *vec)
{
	return (vec->config & V_NOAUTOSHRINK);
}
static inline
bool vinternal_c_allowoutofbounds(Vec *vec)
{
	return (vec->config & V_ALLOWOUTOFBOUNDS);
}
static inline
bool vinternal_c_iternocopy(Vec *vec)
{
	return (vec->config & V_ITERNOCOPY);
}
static inline
bool vinternal_c_exactsizing(Vec *vec)
{
	return (vec->config & V_EXACTSIZING);
}
static inline
bool vinternal_c_rawnocopy(Vec *vec)
{
	return (vec->config & V_RAWNOCOPY);
}
static inline
bool vinternal_c_keepoffset(Vec *vec)
{
	return (vec->config & V_KEEPOFFSET);
}


/**
 * Overflow checked double operation on size_t.
 */
static inline
size_t vinternal_size_t_double(size_t n)
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

/**
 * Overflow checked addition of two size_t.
 */
static inline
size_t vinternal_size_t_add(size_t x, size_t y)
{
	return x > ((size_t) SIZE_MAX) - y
		? (size_t) SIZE_MAX
		: x + y;
}

/**
 * Overflow checked subtraction of two size_t.
 */
static inline
size_t vinternal_size_t_sub(size_t x, size_t y)
{
	return x > y
		? 0
		: x - y;
}

static inline // DEPRECATED
bool vinternal_is_power_of_two(size_t n)
{
	return n != 0 && (n & (n - 1)) == 0;
}

/**
 * Align size to be a multiple of alignment.
 */
static inline // DEPRECATED
size_t vinternal_align(size_t size, size_t alignment)
{
	/*
	 * 1. Add alignment - 1 to original size to make sure the type will fit into its new size
	 * 2. Zero all bits after the high bit of alignment to make the result divisible by alignment
	 */
	return (((size + alignment - 1)) & ~(alignment - 1));
}

/**
 * Returns real capacity of vector depending on offset and config.
 */
static inline
size_t vinternal_real_cap(Vec *vec)
{
	return vinternal_c_keepoffset(vec)
		? vec->cap - vec->offset
		: vec->cap;
}

/**
 * Returns real element size of vector without alignment padding.
 */
/*static inline // DEPRECATED
size_t vinternal_real_elem_size(Vec *vec)
{
	return vec->aligned
		? vec->elem_size - vec->alignment_padding_size
		: vec->elem_size;
}*/
/*
static inline // DEPRECATED
void *vinternal_aligned_to_buffer_memcpy(void *restrict dest, const void *restrict src, size_t n, size_t elem_size, size_t aligned_size)
{
	for (size_t i = 0, j = 0; i < n; i += aligned_size, j += elem_size)
	{
		memcpy(
			((char *) dest) + j,
			((char *) src) + i,
			elem_size);
	}

	return dest;
}

static inline // DEPRECATED
void *vinternal_buffer_to_aligned_memcpy(void *restrict dest, const void *restrict src, size_t n, size_t elem_size, size_t aligned_size)
{
	for (size_t i = 0, j = 0; i < n; i += aligned_size, j += elem_size)
	{
		memcpy(
			((char *) dest) + i,
			((char *) src) + j,
			elem_size);
	}

	return dest;
}
*/
/**
 * Create a new vector.
 */
static
Vec *vinternal_create(size_t elem_size, size_t base_cap)
{
	Vec *vec = malloc(sizeof(Vec));
	if (vec == NULL)
		return NULL;

	if (base_cap == 0)
	{
		vec->data = NULL;
	}
	else
	{
		vec->data = malloc(elem_size * base_cap);
		if (vec->data == NULL)
		{
			free(vec);
			return NULL;
		}
	}

	vec->elem_size = elem_size;

	vec->len = 0;
	vec->cap = base_cap;

	vec->first = vec->last = vec->data;

	vec->offset = 0;

	vec->config = vinternal_base_cfg;

	return vec;
}

/**
 * Clone a vector.
 */
static
Vec *vinternal_clone(Vec *vec, bool reduced)
{
	Vec *clone = malloc(sizeof(Vec));
	if (clone == NULL)
		return NULL;

	size_t clone_cap = reduced ? vec->len : vec->cap;

	clone->data = malloc(clone_cap * vec->elem_size);
	if (clone->data == NULL)
	{
		free(clone);
		return NULL;
	}

	clone->elem_size = vec->elem_size;
	clone->len = vec->len;
	clone->cap = clone_cap;
	clone->offset = reduced ? 0 : vec->offset;

	clone->first = memcpy(
		((char *) clone->data) + (clone->offset * clone->elem_size),
		reduced ? vec->first : vec->data,
		(vec->offset + vec->len) * vec->elem_size);

	clone->last = ((char *) clone->first) + (clone->len * clone->elem_size);

	clone->config = vec->config;

	return clone;
}

/**
 * Zeroes the offset of a vector only if that is
 * enough to reach min_cap and config allows it.
 * Returns true if offset has been zeroed.
 */
static inline
bool vinternal_zero_offset_maybe(Vec *vec, size_t min_cap)
{
	if (vec->cap < min_cap || vec->offset == 0)
		return false;

	if (vec->cap - vec->offset < min_cap && !vinternal_c_keepoffset(vec))
	{
		memmove(
			vec->data,
			vec->first,
			vec->len * vec->elem_size);

		vec->offset = 0;

		return true;
	}

	return false;
}
/**
 * Resize a vector to size if possible and respecting its config.
 */
static
int vinternal_set_size(Vec *vec, size_t size, bool keep_offset, bool prefer_memmove) // TODO: offset handling
{
	if (size == 0)
	{
		v_clear(vec);
		return VE_OK;
	}

	if (vec->cap == 0)
	{
		void *data = malloc(size * vec->elem_size);

		if (data == NULL)
			return VE_NOMEM;

		vec->data = data;
	}
	else if (vec->offset == 0 || keep_offset)
	{
		if (vinternal_real_cap(vec) == size)
			return VE_OK;

		void *new_data = realloc(vec->data, (size + vec->offset) * vec->elem_size);

		if (new_data == NULL)
			return VE_NOMEM;

		vec->data = new_data;
	}
	else if (vec->cap >= size && prefer_memmove) // ???
	{
		vec->offset = vec->cap - size;

		vec->first = memmove(
			((char *) vec->data) + (vec->offset * vec->elem_size),
			vec->first,
			vec->len);
	}
	else
	{
		void *new_data = malloc(size * vec->elem_size);

		if (new_data == NULL)
			return VE_NOMEM;

		memcpy(
			new_data,
			((char *) vec->data) + (vec->offset * vec->elem_size),
			vec->len * vec->elem_size);

		free(vec->data);

		vec->data = new_data;
		vec->offset = 0;
	}

	vec->first = ((char *) vec->data) + (vec->offset * vec->elem_size);
	vec->last = ((char *) vec->first) + (vec->len * vec->elem_size);

	vec->cap = size;

	return VE_OK;
}
/**
 * Grows the vector to the required capacity
 * if config allows it.
 */
static inline
int vinternal_grow_maybe(Vec *vec, size_t min_cap, bool keep_offset)
{
	size_t new_cap, real_cap;
	new_cap = real_cap = vinternal_real_cap(vec);

	while (min_cap > new_cap)
		new_cap = vinternal_size_t_double(new_cap);

	if (new_cap == real_cap)
		return VE_OK;

	if (vinternal_c_noautogrow(vec))
		return VE_NOCAP;

	VMACRO_RETURN_MAYBE(
		vinternal_set_size(vec, new_cap, keep_offset, true));

	return VE_OK;
}
/**
 * Shrinks the vector if it makes sense
 * and config allows it.
 */
static inline
int vinternal_shrink_maybe(Vec *vec) // TODO: account for offset in loop?
{
	size_t half_cap, new_cap, real_cap;
	half_cap = new_cap = real_cap = vinternal_real_cap(vec);

	while (vec->len <= (half_cap >>= 1))
		new_cap = half_cap;

	if (new_cap == real_cap)
		return VE_OK;

	if (vinternal_c_noautoshrink(vec))
		return VE_OK;

	VMACRO_RETURN_MAYBE(
		vinternal_set_size(vec, new_cap, vinternal_c_keepoffset(vec), false));

	return VE_OK;
}

/**
 * General logging function with timestamp.
 */
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

		tag, msg);
}
/**
 * Log to vinternal_out_stream.
 */
static inline
void vinternal_log(char *tag, char *msg)
{
	vinternal_log_to_stream(
		vinternal_out_stream
		? vinternal_out_stream
		: stdout,
		tag, msg);
}
/**
 * Log to vinternal_err_stream.
 */
static inline
void vinternal_err(char *tag, char *msg)
{
	vinternal_log_to_stream(
		vinternal_err_stream
		? vinternal_err_stream
		: stderr,
		tag, msg);
}
/**
 * Log to vinternal_out_stream.
 * Only logs in verbose mode.
 */
static inline
void vinternal_vlog(char *tag, char *msg)
{
	if (vinternal_verbose)
		vinternal_log(tag, msg);
}
/**
 * Log to vinternal_err_stream.
 * Only logs in verbose mode.
 */
static inline
void vinternal_verr(char *tag, char *msg)
{
	if (vinternal_verbose)
		vinternal_err(tag, msg);
}


// Implementations functions exposed to user ; doc in vector.h

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


Vec *v_create(size_t elem_size)
{
	return vinternal_create(elem_size, vinternal_base_cap);
}

Vec *v_create_with(size_t elem_size, size_t base_cap)
{
	return vinternal_create(elem_size, base_cap);
}


int v_set_cfg(Vec *vec, enum VecCfg config)
{
	if (vec == NULL)
		return VE_INVAL;

	vec->config = config;

	return VE_OK;
}

int v_add_cfg(Vec *vec, enum VecCfg config)
{
	if (vec == NULL)
		return VE_INVAL;

	vec->config |= config;

	return VE_OK;
}

int v_remove_cfg(Vec *vec, enum VecCfg config)
{
	if (vec == NULL)
		return VE_INVAL;

	vec->config &= ~config;

	return VE_OK;
}


size_t v_elem_size(Vec *vec)
{
	return (vec == NULL) ? 0 : vec->elem_size;
}

size_t v_len(Vec *vec)
{
	return (vec == NULL) ? 0 : vec->len;
}

size_t v_cap(Vec *vec)
{
	return (vec == NULL) ? 0 : vec->cap;
}

size_t v_offset(Vec *vec)
{
	return (vec == NULL) ? 0 : vec->offset;
}


int v_clear(Vec *vec)
{
	if (vec == NULL)
		return VE_INVAL;

	free(vec->data);

	vec->data = vec->first = vec->last = NULL;
	vec->len = vec->cap = vec->offset = 0;

	return VE_OK;
}


int v_set_size(Vec *vec, size_t size)
{
	if (vec == NULL)
		return VE_INVAL;

	if (size < vec->len)
		return VE_TOOLONG;

	bool prefer_memmove = (vec->cap - vec->offset > size);

	return vinternal_set_size(vec, size, vinternal_c_keepoffset(vec), prefer_memmove);
}

int v_reduce(Vec *vec)
{
	if (vec == NULL)
		return VE_INVAL;

	return vinternal_set_size(vec, vec->len, vinternal_c_keepoffset(vec), false);
}

int v_reduce_strict(Vec *vec) // remove???
{
	if (vec == NULL)
		return VE_INVAL;

	return vinternal_set_size(vec, vec->len, false, false);
}

int v_grow(Vec *vec, size_t by_size)
{
	if (vec == NULL)
		return VE_INVAL;

	size_t new_cap = vinternal_size_t_add(vec->cap, by_size);

	return vinternal_set_size(vec, new_cap, vinternal_c_keepoffset(vec), true);
}

int v_shrink(Vec *vec, size_t by_size)
{
	if (vec == NULL)
		return VE_INVAL;

	size_t new_cap = vinternal_size_t_sub(vec->cap, by_size);

	if (new_cap < vec->len)
		return VE_TOOLONG;

	return vinternal_set_size(vec, new_cap, vinternal_c_keepoffset(vec), false);
}


int v_push(Vec *vec, void *elem)
{
	if (vec == NULL)
		return VE_INVAL;

	if (elem == NULL)
		return VE_OK;

	size_t new_len = vec->len + 1;

	VMACRO_RETURN_MAYBE(
		vinternal_grow_maybe(vec, new_len, vinternal_c_keepoffset(vec)));

	vec->len = new_len;

	memcpy(
		vec->last,
		elem,
		vec->elem_size);

	vec->last = ((char *) vec->last) + vec->elem_size;

	return VE_OK;
}

int v_pop(Vec *vec, void *dest)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	vec->last = ((char*) vec->last) - vec->elem_size;
	vec->len--;

	if (dest != NULL)
	{
		memcpy(
			dest,
			vec->last,
			vec->elem_size);
	}

	VMACRO_RETURN_MAYBE(
		vinternal_shrink_maybe(vec));

	return VE_OK;
}


int v_first(Vec *vec, void *dest)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	if (dest == NULL)
		return VE_OK;

	memcpy(
		dest,
		vec->first,
		vec->elem_size);

	return VE_OK;
}

int v_last(Vec *vec, void *dest)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	if (dest == NULL)
		return VE_OK;

	memcpy(
		dest,
		((char *) vec->last - vec->elem_size),
		vec->elem_size);

	return VE_OK;
}


int v_at(Vec *vec, void *dest, size_t index)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index > vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
			return VE_OUTOFBOUNDS;

		return v_last(vec, dest);
	}

	if (dest == NULL)
		return VE_OK;

	memcpy(
		dest,
		((char *) vec->first) + index * vec->elem_size,
		vec->elem_size);

	return VE_OK;
}

int v_insert(Vec *vec, void *elem, size_t index) // WIP
{
	if (vec == NULL)
		return VE_INVAL;

	if (elem == NULL)
		return VE_OK;

	size_t index_plus_one = index + 1;

	if (index >= vinternal_real_cap(vec))
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_push(vec, elem);

		VMACRO_RETURN_MAYBE(
			vinternal_grow_maybe(vec, index_plus_one, vinternal_c_keepoffset(vec)));
	}

	size_t size_til_index = index * vec->elem_size;

	if (index >= vec->len)
	{
		if (!vinternal_c_allowoutofbounds(vec))
			return v_push(vec, elem);

		if (vec->len == 0)
		{
			vec->first = ((char *) vec->data) + (vec->offset * vec->elem_size);
		}

		memset(
			((char *) vec->last),
			0,
			(index - vec->len) * vec->elem_size);

		vec->last = ((char *) memcpy(
			((char *) vec->first) + size_til_index,
			elem,
			vec->elem_size))
			+ vec->elem_size;

		vec->len = index_plus_one;

		return VE_OK;
	}

	size_t new_len = vec->len + 1;
	
	VMACRO_RETURN_MAYBE(
		vinternal_grow_maybe(vec, new_len, vinternal_c_keepoffset(vec)));

	memmove(
		((char *) vec->first) + (index_plus_one * vec->elem_size),
		((char *) vec->first) + size_til_index,
		(vec->len - index) * vec->elem_size);

	memcpy(
		((char *) vec->first) + size_til_index,
		elem,
		vec->elem_size);

	vec->len = new_len;

	vec->last = ((char *) vec->last) + vec->elem_size;

	return VE_OK;
}

int v_remove(Vec *vec, void *dest, size_t index) // WIP
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index >= vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
			return VE_OUTOFBOUNDS;

		return v_pop(vec, dest);
	}

	if (dest != NULL)
	{
		memcpy(
			dest,
			((char *) vec->first) + (index * vec->elem_size),
			vec->elem_size);
	}

	memmove(
		((char *) vec->first) + (index * vec->elem_size),
		((char *) vec->first) + ((index + 1) * vec->elem_size),
		(vec->len - index) * vec->elem_size);

	vec->len--;

	vec->last = ((char *) vec->last) - vec->elem_size;

	VMACRO_RETURN_MAYBE(
		vinternal_shrink_maybe(vec));

	return VE_OK;
}


int v_swap_insert(Vec *vec, void *elem, size_t index)
{
	if (vec == NULL)
		return VE_INVAL;

	if (elem == NULL)
		return VE_OK;

	if (index >= vec->len)
		return v_insert(vec, elem, index);

	VMACRO_RETURN_MAYBE(
		v_push(vec, ((char *) vec->first) + (index * vec->elem_size)));

	memcpy(
		((char *) vec->first) + (index * vec->elem_size),
		elem,
		vec->elem_size);

	return VE_OK;
}

int v_swap_remove(Vec *vec, void *dest, size_t index)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len == 0)
		return VE_EMPTY;

	if (index >= vec->len)
	{
		if (vinternal_c_allowoutofbounds(vec))
			return VE_OUTOFBOUNDS;

		return v_pop(vec, dest);
	}

	void *removed = ((char *)vec->first) + (index * vec->elem_size);

	if (dest != NULL)
	{
		memcpy(
			dest,
			removed,
			vec->elem_size);
	}

	VMACRO_RETURN_MAYBE(
		v_pop(vec, removed));

	return VE_OK;
}


void *v_raw(Vec *vec)
{
	if (vec == NULL)
		return NULL;

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
	if (vec == NULL)
		return NULL;

	if (vec->len == 0)
		return NULL;

	if (from >= to)
		return NULL;

	if (from >= vec->len)
		return NULL;

	if (to >= vec->len)
		to = vec->len - 1;


	if (vinternal_c_rawnocopy(vec))
		return ((char *) vec->first) + (from * vec->elem_size);

	size_t raw_slice_size = (to - from) * vec->elem_size;

	void *raw_slice = malloc(raw_slice_size);
	if (raw_slice == NULL)
		return NULL;

	return memcpy(
			raw_slice,
			((char *) vec->first) + (from * vec->elem_size),
			raw_slice_size);
}


Vec *v_slice(Vec *vec, size_t from, size_t to)
{
	if (vec == NULL)
		return NULL;

	if (vec->len == 0)
		return NULL;

	if (from >= to)
		return NULL;

	if (from >= vec->len)
		return NULL;

	if (to >= vec->len)
		to = vec->len - 1;

	size_t slice_len = to - from;
	size_t slice_size = slice_len * vec->elem_size;

	Vec *slice = vinternal_create(
		vec->elem_size, slice_len);

	memcpy(
		slice->data,
		((char*)vec->first) + (from * vec->elem_size),
		slice_size);

	slice->elem_size = vec->elem_size;
	slice->len = slice_len;
	slice->first = slice->data;
	slice->last = ((char*)slice->first) + (slice->len * vec->elem_size);

	slice->config = vec->config;

	return slice;
}

// rework later?
int v_prepend(Vec *vec, void *src, size_t amount)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->offset >= amount)
	{
		vec->offset -= amount;
		vec->first = memcpy(
			((char *) vec->data) + (vec->offset * vec->elem_size),
			src,
			amount * vec->elem_size);

		return VE_OK;
	}

	size_t new_len = vec->len + amount;

	VMACRO_RETURN_MAYBE(
		vinternal_grow_maybe(vec, new_len, true));

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

	vec->last = ((char *)vec->first) + (vec->len * vec->elem_size);

	return VE_OK;
}
// rework later?
int v_append(Vec *vec, void *src, size_t amount)
{
	if (vec == NULL)
		return VE_INVAL;

	if (src == NULL || amount == 0)
		return VE_OK;

	size_t new_len = vec->len + amount;

	VMACRO_RETURN_MAYBE(
		vinternal_grow_maybe(vec, new_len, true));

	if (vec->len == 0)
	{
		vec->first = ((char *)vec->data) + (vec->offset * vec->elem_size);
		vec->last = vec->first;
	}

	vec->len = new_len;

	size_t new_size = amount * vec->elem_size;

	memcpy(
		vec->last,
		src,
		new_size);

	vec->last = ((char *)vec->last) + new_size;

	return VE_OK;
}


int v_trim_front(Vec *vec, void *dest, size_t amount)
{
	if (vec == NULL)
		return VE_INVAL;

	if (vec->len < amount)
		amount = vec->len;

	size_t trim_size = amount * vec->elem_size;

	if (dest != NULL)
	{
		memcpy(
			dest,
			vec->first,
			trim_size);
	}

	vec->first = ((char *)vec->first) + trim_size;
	vec->len -= amount;

	VMACRO_RETURN_MAYBE(
		vinternal_shrink_maybe(vec));

	return VE_OK;
}


Vec *v_clone(Vec *vec)
{
	if (vec == NULL)
		return NULL;

	return vinternal_clone(vec, false);
}

Vec *v_reduced_clone(Vec *vec)
{
	if (vec == NULL)
		return NULL;

	return vinternal_clone(vec, true);
}

int v_zero(Vec *vec)
{
	if (vec == NULL)
		return VE_INVAL;

	memset(vec->first, 0, (vec->len * vec->elem_size));

	return VE_OK;
}

int v_softclear(Vec *vec)
{
	if (vec == NULL)
		return VE_INVAL;

	vec->len = 0;

	if (!vinternal_c_keepoffset(vec))
	{
		vec->offset = 0;
		vec->first = vec->data;
	}

	vec->last = vec->first;

	return VE_OK;
}

int v_destroy(Vec *vec)
{
	if (vec == NULL)
		return VE_INVAL;

	free(vec->data);
	free(vec);

	return VE_OK;
}


VecIter *v_iter(Vec *vec)
{
	if (vec == NULL)
		return NULL;

	VecIter *iter = malloc(sizeof(VecIter));
	if (iter == NULL)
		return NULL;

	iter->pos = 0;

	if (vec->len == 0)
	{
		iter->vec = NULL;
		iter->finger = NULL;
		iter->owner = false;

		return iter;
	}

	if (vinternal_c_iternocopy(vec))
	{
		iter->vec = vec;
		iter->finger = vec->first;
		iter->owner = true;

		return iter;
	}

	iter->vec = vinternal_clone(vec, true);
	if (iter->vec == NULL)
		return NULL;

	iter->owner = false;

	return iter;
}

VecIter *v_into_iter(Vec **restrict vec)
{
	if (vec == NULL)
		return NULL;

	VecIter *iter = malloc(sizeof(VecIter));

	iter->pos = 0;

	if ((*vec)->len == 0)
	{
		iter->vec = NULL;
		iter->finger = NULL;
		iter->owner = false;

		return iter;
	}

	iter->vec = *vec;
	*vec = NULL;
	iter->finger = iter->vec->first;
	iter->owner = true;

	return iter;
}


int vi_destroy(VecIter *iter)
{
	if (iter == NULL)
		return VE_INVAL;

	if (iter->vec != NULL && iter->owner)
	{
		free(iter->vec->data);
		free(iter->vec);
	}

	free(iter);

	return VE_OK;
}


bool vi_is_owner(VecIter *iter)
{
	if (iter == NULL)
		return false;

	return iter->owner;
}

size_t vi_pos(VecIter *iter)
{
	if (iter == NULL)
		return 0;

	return iter->pos;
}

bool vi_done(VecIter *iter)
{
	if (iter == NULL)
		return true;

	if (iter->vec == NULL || iter->vec->len == 0)
		return true;

	return (iter->pos == iter->vec->len - 1);
}


int vi_next(VecIter *iter, void *dest) // TODO -> return values?
{
	if (iter == NULL)
		return VE_INVAL;

	if (iter->finger == NULL)
	{
		return 1;
	}

	memcpy(
		dest,
		iter->finger,
		iter->vec->elem_size);

	iter->pos++;

	if (iter->pos >= iter->vec->len - 1)
	{
		iter->pos = iter->vec->len - 1;
		iter->finger = NULL;
	} else {
		iter->finger = ((char *)iter->finger) + iter->vec->elem_size;
	}

	return VE_OK;
}

int vi_skip(VecIter *iter, size_t amount)
{
	if (iter == NULL)
		return VE_INVAL;

	iter->pos += amount;

	if (iter->pos >= iter->vec->len - 1)
	{
		iter->pos = iter->vec->len - 1;
		iter->finger = NULL;
	}
	else
	{
		iter->finger = ((char*)iter->finger) + (iter->vec->elem_size * iter->pos);
	}

	return VE_OK;
}

int vi_goto(VecIter *iter, size_t index)
{
	if (iter == NULL)
		return VE_INVAL;

	iter->pos = index;

	if (iter->pos >= iter->vec->len - 1)
	{
		iter->pos = iter->vec->len - 1;
		iter->finger = NULL;
	}
	else
	{
		iter->finger = ((char*)iter->vec->first) + (iter->vec->elem_size * iter->pos);
	}

	return VE_OK;
}


Vec *vi_from_iter(VecIter *iter)
{
	if (iter == NULL)
		return NULL;

	if (iter->vec == NULL)
		return NULL;

	if (iter->owner)
	{
		Vec *vec = iter->vec;

		free(iter);

		return vec;
	}

	Vec *vec = vinternal_clone(iter->vec, true);
	if (vec == NULL)
		return NULL;

	vi_destroy(iter);

	return vec;
}


#undef VMACRO_RETURN_MAYBE
