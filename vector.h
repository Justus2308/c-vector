#ifndef C_VECTOR_H_
#define C_VECTOR_H_


#include <stdbool.h>
#include <stddef.h>

/**
 * A vector that will grow and shrink automatically to
 * accomodate all elements passed to it.
 * One vector can only operate on a single element size.
 */
typedef struct vinternal_Vec Vec;

/**
 * An iterator over a specified vector.
 */
typedef struct vinternal_VecIter VecIter;


/**
 * The default base element capacity of vectors created with v_create
 */
#define VEC_DEFAULT_BASE_CAP 8


enum VecCfg
{
	/**
	 * Pass this flag to a set*cfg function
	 * (without simultaneously passing any other flags)
	 * to reset the target's configuration.
	 */
	V_RESETCFG = 0,

	/**
	 * All functions will return an error instead of 
	 * automatically growing it if the vector is too
	 * small to perform their intended operation.
	 * If this flag is set, the vector's capacity
	 * has to be managed manually.
	 */
	V_NOAUTOGROW = 1 << 0,

	/**
	 * The vector will not be shrunk automatically
	 * when enough unused capacity gets detected.
	 */
	V_NOAUTOSHRINK = 1 << 1,

	/**
	 * After an error has occured, all successive operations on the vector
	 * will also fail until the error gets handled using v_error().
	 */
	V_CARRYERROR = 1 << 2,

	/**
	 * All insert/remove functions will accept indices that
	 * are out-of-bounds instead of converting it to the last
	 * index of the current vector if this flag is set.
	 * If such an index is passed, insert functions will
	 * grow the vector accordingly, insert the specified value
	 * at its index and fill all unassigned entries up to the
	 * index with zeroes.
	 * Remove functions and the v_at() function will return
	 * an error if the passed index is out-of-bounds.
	 */
	V_ALLOWOUTOFBOUNDS = 1 << 3,

	/**
	 * All iterators over a vector will not create their own copy of
	 * it and instead iterate over the original vector.
	 * Changes in the original vector will influence its iterators.
	 */
	V_ITERNOCOPY = 1 << 4,

	/**
	 * The vector will always have only as much capacity as it currently needs.
	 * This might save memory, but it comes at the cost of more reallocations.
	 */
	V_EXACTSIZING = 1 << 5,

	/**
	 * All raw functions will return a pointer to the actual data of the
	 * specified vector instead of returning a copy of the data.
	 */
	V_RAWNOCOPY = 1 << 6,

	/**
	 * When resizing the vector the current offset into the pure
	 * data stream will be kept.
	 * This can be useful if a lot trim_front followed by a lot
	 * of prepend operations are expected.
	 */
	V_KEEPOFFSET = 1 << 7,
};


/**
 * Set the base configuration of newly created vectors.
 * Multiple config flags can be combined with '|'.
 */
extern void vc_set_base_cfg(enum VecCfg config);

/**
 * Set the base capacity of newly created vectors.
 * The default value is VEC_DEFAULT_BASE_CAP.
 */
extern void vc_set_base_cap(size_t base_cap);


/**
 * Create a new vector with a custom base capacity.
 */
extern Vec *v_create_with(size_t elem_size, size_t cap);

/**
 * Create a new vector with elements of a specified size.
 */
extern Vec *v_create(size_t elem_size);


/**
 * Set the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 */
extern void v_set_cfg(Vec *vec, enum VecCfg config);

/**
 * Add flags to the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 */
extern void v_add_cfg(Vec *vec, enum VecCfg config);

/**
 * Remove flags from the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 */
extern void v_remove_cfg(Vec *vec, enum VecCfg config);


/**
 * Returns the element size of the specified vector.
 */
extern size_t v_elem_size(Vec *vec);

/**
 * Returns the current length of the specified vector.
 */
extern size_t v_len(Vec *vec);

/**
 * Returns the current capacity of the specified vector.
 */
extern size_t v_cap(Vec *vec);

/**
 * Should be used to check for errors on macros
 * which take a type as a parameter since there is
 * no way to universally communicate that something
 * has gone wrong through the return value.
 * Every function that fails will set the internal
 * error field of the respective vector.
 * A nonzero value means that an error has occured.
 */
extern int v_error(Vec *vec);


/**
 * Tries to resize the specified vector.
 * If the vector is longer than the specified size,
 * its capacity gets trimmed to its exact length.
 * If an error occurs, a non-zero value is returned.
 */
extern int v_set_size(Vec *vec, size_t size);

/**
 * Reduces the specified vector's capacity to
 * its exact length.
 * If an error occurs, a non-zero value is returned.
 */
extern int v_reduce(Vec *vec);

/**
 * Grows the specified vector.
 * If an error occurs, a non-zero value is returned.
 */
extern int v_grow(Vec *vec, size_t by_size);

/**
 * Tries to shrink the specified vector.
 * If the vector is longer than the specified size
 * its capacity gets trimmed to its exact length.
 * If an error occurs, a non-zero value is returned.
 */
extern int v_shrink(Vec *vec, size_t by_size);

extern int v_push(Vec *vec, void *elem);
extern void *v_pop_ptr(Vec *vec);
#define v_pop(vec, type) *((type *) v_pop_ptr(vec))

extern void *v_first_ptr(Vec *vec);
#define v_first(vec, type) *((type *) v_first_ptr(vec))
extern void *v_last_ptr(Vec *vec);
#define v_last(vec, type) *((type *) v_last_ptr(vec))
extern void *v_at_ptr(Vec *vec, size_t index);
#define v_at(vec, index, type) *((type *) v_at_ptr(vec, index))

// use front ptr for remove(vec, 0) to avoid memmove
extern int v_insert(Vec *vec, size_t index, void *elem);
extern void *v_remove_ptr(Vec *vec, size_t index);
#define v_remove(vec, index, type) *((type *) v_remove_ptr(vec, index))

extern int v_swap_insert(Vec *vec, size_t index, void *elem);
extern void *v_swap_remove_ptr(Vec *vec, size_t index);
#define v_swap_remove(vec, index, type) *((type *) v_swap_remove_ptr(vec, index))

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
extern void v_softclear(Vec *vec);
extern void v_clear(Vec *vec);
extern void v_destroy(Vec *vec);

extern VecIter *v_iter(Vec *vec);
extern VecIter *v_into_iter(Vec **restrict vec);

extern bool vi_is_owner(VecIter *iter);
extern size_t vi_pos(VecIter *iter);

extern void *vi_next_ptr(VecIter *iter);
#define vi_next(iter, type) *((type *) vi_next(iter))

extern void vi_skip(VecIter *iter, size_t amount);
extern void vi_goto(VecIter *iter, size_t index);

extern Vec *vi_from_iter(VecIter *iter);
extern void vi_destroy(VecIter *iter);


#endif // C_VECTOR_H_
