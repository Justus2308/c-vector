#pragma once

#include <stdbool.h>
#include <stddef.h>


typedef struct vec Vec;
typedef struct vec_iter VecIter;

enum VecCfg
{
	VRESETCFG = 0,

	VNOAUTOGROW = 1,
	VNOAUTOSHRINK = 2,
	VSOFTCLEAR = 4,
	VALLOWOUTOFBOUNDS = 8,
	VITERNOCOPY = 16,
	VITERSELFDESTRUCT = 32,
	VRAWNOCOPY = 64,
	VTHREADSAFE = 128, /* TODO */
};


extern void vc_set_base_cfg(enum VecCfg config);
extern void vc_set_base_cap(size_t base_cap);
extern void vc_set_allocator(
	void *(*malloc)(size_t),
	void *(*calloc)(size_t, size_t),
	void *(*realloc)(void *, size_t),
	void (*free)(void *));

extern Vec *v_create_with(size_t elem_size, size_t cap);
extern Vec *v_create(size_t elem_size);

extern void v_set_cfg(Vec *vec, enum VecCfg config);
extern void v_add_cfg(Vec *vec, enum VecCfg config);
extern void v_remove_cfg(Vec *vec, enum VecCfg config);

extern size_t v_elem_size(Vec *vec);
extern size_t v_len(Vec *vec);
extern size_t v_cap(Vec *vec);

extern int v_set_size(Vec *vec, size_t size);
extern int v_grow(Vec *vec, size_t by_size);
extern int v_shrink(Vec *vec, size_t by_size);

extern int v_push(Vec *vec, void *elem);
extern void *v_pop(Vec *vec);

extern void *v_first(Vec *vec);
extern void *v_last(Vec *vec);

extern void *v_at(Vec *vec, size_t index);
// use front ptr for remove(vec, 0) to avoid memmove
extern int v_insert(Vec *vec, size_t index, void *elem);
extern void *v_remove(Vec *vec, size_t index);

extern int v_swap_insert(Vec *vec, size_t index, void *elem);
extern void *v_swap_remove(Vec *vec, size_t index);

extern void *v_raw(Vec *vec);
extern void *v_raw_slice(Vec *vec, size_t from, size_t to);

extern Vec *v_slice(Vec *vec, size_t from, size_t to);

extern int v_prepend(Vec *vec, void *src, size_t len);
extern int v_append(Vec *vec, void *src, size_t len);
// use front ptr to lazily trim front without reallocating or memmoving
extern int v_trim_front(Vec *vec, size_t amount);
extern int v_trim_back(Vec *vec, size_t amount);

extern int v_insert_multiple(Vec *vec, void *src, size_t len);
extern void *v_remove_multiple(Vec *vec, size_t from, size_t to);

extern Vec *v_split(Vec *vec, size_t at_index);

extern Vec *v_clone(Vec *vec);
extern void v_zero(Vec *vec);
extern void v_clear(Vec *vec);
extern void v_destroy(Vec *vec);

extern VecIter *v_iter(Vec *vec);
extern VecIter *v_into_iter(Vec **restrict vec);

extern bool vi_is_owner(VecIter *iter);
extern size_t vi_pos(VecIter *iter);

extern void *vi_next(VecIter *iter);
extern void vi_skip(VecIter *iter, size_t amount);
extern void vi_goto(VecIter *iter, size_t index);

extern Vec *vi_from_iter(VecIter *iter);
extern void vi_destroy(VecIter *iter);
