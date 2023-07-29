#ifndef C_VECTOR_H_
#define C_VECTOR_H_

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>


/**
 * A vector that will grow and shrink automatically to
 * accomodate all elements passed to it.
 * One unique vector can only store elements of the same size.
 */
typedef struct vinternal_Vec Vec;

/**
 * An iterator over a specified vector.
 */
typedef struct vinternal_VecIter VecIter;


/**
 * The default base configuration of vectors created with v_create or v_create_with
 */
#define VEC_DEFAULT_BASE_CFG 0

/**
 * The default base element capacity of vectors created with v_create
 */
#define VEC_DEFAULT_BASE_CAP 8


/**
 * Vector configuration flags
 */
enum VecCfg
{
	/**
	 * Pass this flag to a set*cfg function
	 * (without simultaneously passing any other flags)
	 * to reset the target's configuration.
	 */
	V_RESETCFG			= 0,

	/**
	 * All functions will return an error instead of 
	 * automatically growing it if the vector is too
	 * small to perform their intended operation.
	 * If this flag is set, the vector's capacity
	 * has to be managed manually.
	 */
	V_NOAUTOGROW		= 1 << 0,

	/**
	 * The vector will not be shrunk automatically
	 * when enough unused capacity gets detected.
	 */
	V_NOAUTOSHRINK		= 1 << 1,

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
	V_ALLOWOUTOFBOUNDS	= 1 << 2,

	/**
	 * All iterators over a vector will not create their own copy of
	 * it and instead iterate over the original vector.
	 * Changes in the original vector will influence its iterators.
	 */
	V_ITERNOCOPY		= 1 << 3,

	/**
	 * The vector will always have only as much capacity as it currently needs.
	 * This might save memory, but it comes at the cost of more reallocations.
	 */
	V_EXACTSIZING		= 1 << 4,

	/**
	 * All raw functions will return a pointer to the actual data of the
	 * specified vector instead of returning a copy of the data.
	 */
	V_RAWNOCOPY			= 1 << 5,

	/**
	 * When resizing the vector the current offset into the pure
	 * data stream will be kept.
	 * This can be useful if a lot trim_front followed by a lot
	 * of prepend operations are expected.
	 */
	V_KEEPOFFSET		= 1 << 6,
};

/**
 * Vector error codes
 */
enum VecErr
{
	/**
	 * No error has occured.
	 */
	VE_OK = 0,

	/**
	 * Not enough memory available.
	 */
	VE_NOMEM,

	/**
	 * The vector is empty.
	 */
	VE_EMPTY,

	/**
	 * Tried to access out of bounds index.
	 */
	VE_OUTOFBOUNDS,

	/**
	 * The vector is too long.
	 */
	VE_TOOLONG,

	/**
	 * The vector doesn't have enough capacity and is not allowed to grow.
	 */
	VE_NOCAP,

	/**
	 * The vector/iterator pointer is invalid.
	 */
	VE_INVAL,

	/**
	 * For internal use only.
	 * This value will never be returned by a function as an error code.
	 */
	VINTERNAL_LAST,
};


/**
 * Set the base configuration of newly created vectors.
 * Multiple config flags can be combined with '|'.
 * 
 * @param	config	Desired configuration
 * 
 * @see		VecCfg
 */
extern void vc_set_base_cfg(enum VecCfg config);

/**
 * Set the base capacity of newly created vectors.
 * The default value is VEC_DEFAULT_BASE_CAP.
 * 
 * @param	base_cap	Desired base capacity
 */
extern void vc_set_base_cap(size_t base_cap);

/**
 * Set output stream for log messages in verbose mode.
 * 
 * @param	stream	The stream/file to write to
 * 
 * @see		vc_set_verbose
 */
extern void vc_set_output_stream(FILE *stream);

/**
 * Set error stream for log messages in verbose mode and v_perror.
 * 
 * @param	stream	The stream/file to write to
 * 
 * @see		vc_set_verbose
 * @see		v_perror
 */
extern void vc_set_error_stream(FILE *stream);

/**
 * Log additional information to stdout/stderr or to specified streams.
 * 
 * @param	verbose		Desired verbose mode setting
 */
extern void vc_set_verbose(bool verbose);


/**
 * Align the size of a type to the pointer size of your system.
 * 
 * @param	size	Size of the type to be aligned
 * @return			Multiple of the system pointer size
 */
inline size_t v_align_to_ptr(size_t size)
{
	/*
	 * 1. Add sizeof(void *) - 1 to original size to make sure the type will fit into its new size
	 * 2. Zero all bits after 0b0100/0b1000 to make the result divisible by sizeof(void *)
	 */
	return (((size + sizeof(void *) - 1)) & ~((size_t) sizeof(void *) - 1));
}


/**
 * Print an appropriate error message for a vector error code.
 * 
 * @param	str		Custom message to be printed before the error message
 * @param	err		Vector error code returned by a vector function
 * 
 * @see		VecErr
 */
extern void v_perror(const char *str, enum VecErr err);


/**
 * Create a new vector with a custom base capacity.
 * 
 * @param	elem_size	Size of the elements to be stored in the vector in bytes
 * @param	cap			Desired base capacity
 * @return				Pointer to a new Vec struct, NULL on error
 *
 * @see Vec
 */
extern Vec *v_create_with(size_t elem_size, size_t cap);

/**
 * Create a new vector with elements of a specified size.
 * 
 * @param	elem_size	Size of the elements to be stored in the vector in bytes
 * @return				Pointer to a new Vec struct, NULL on error
 *
 * @see Vec
 */
extern Vec *v_create(size_t elem_size);


/**
 * Set the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 * 
 * @param	vec		Vector to be operated on
 * @param	config	Desired configuration
 * @return			Non-zero value on error
 * 
 * @see		VecCfg
 */
extern int v_set_cfg(Vec *vec, enum VecCfg config);

/**
 * Add flags to the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 * 
 * @param	vec		Vector to be operated on
 * @param	config	Desired configuration to be added
 * @return			Non-zero value on error
 * 
 * @see		VecCfg
 */
extern int v_add_cfg(Vec *vec, enum VecCfg config);

/**
 * Remove flags from the configuration of the specified vector.
 * Multiple config flags can be combined with '|'.
 * 
 * @param	vec		Vector to be operated on
 * @param	config	Desired configuration to be removed
 * @return			Non-zero value on error
 * 
 * @see		VecCfg
 */
extern int v_remove_cfg(Vec *vec, enum VecCfg config);


/**
 * Returns the element size of the specified vector.
 * 
 * @param	vec		Vector to be operated on
 * @return			Element size of the specified vector, 0 on error
 */
extern size_t v_elem_size(Vec *vec);

/**
 * Returns the current length of the specified vector.
 * 
 * @param	vec		Vector to be operated on
 * @return			Length of the specified vector, 0 on error or if length is 0
 */
extern size_t v_len(Vec *vec);

/**
 * Returns the current capacity of the specified vector.
 * 
 * @param	vec		Vector to be operated on
 * @return			Capacity of the specified vector, 0 on error or if capacity is 0
 */
extern size_t v_cap(Vec *vec);


/**
 * Tries to resize the specified vector.
 * If the vector is longer than the specified size,
 * its capacity gets trimmed to its exact length.
 * 
 * @param	vec		Vector to be operated on
 * @param	size	Desired new size of the vector
 * @return			Non-zero value on error
 * 
 * @see		VecErr
 */
extern int v_set_size(Vec *vec, size_t size);

/**
 * Reduces the specified vector's capacity to
 * its exact length.
 *
 * @param	vec		Vector to be operated on
 * @return			Non-zero value on error
 *
 * @see		VecErr
 */
extern int v_reduce(Vec *vec);
/**
 * Reduces the specified vector's capacity to its
 * exact length. The vector's offset will always
 * be zeroed, regardless of its configuration.
 *
 * @param	vec		Vector to be operated on
 * @return			Non-zero value on error
 *
 * @see		VecErr
 */
extern int v_reduce_strict(Vec *vec);

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

/**
 * Add an element to the end of a vector.
 * 
 * @param	vec		Vector to be operated on
 * @param	elem	Pointer to the element to be pushed
 * @return			Non-zero if an error has occured
 */
extern int v_push(Vec *vec, void *elem);
/**
 * Remove an element from the end of a vector.
 * 
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the removed element will be copied to
 * @return			Non-zero if an error has occured
 * 
 * @see		VecErr
 */
extern int v_pop(Vec *vec, void *dest);

/**
 * Get a copy of the first element of a vector.
 * 
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the first element will be copied to
 * @return			Non-zero if an error has occured
 * 
 * @see		VecErr
 */
extern int v_first(Vec *vec, void *dest);
/**
 * Get a copy of the last element of a vector.
 * 
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the last element will be copied to
 * @return			Non-zero if an error has occured
 * 
 * @see		VecErr
 */
extern int v_last(Vec *vec, void *dest);
/**
 * Get a copy of the element at the specified index of a vector.
 * 
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the specified element will be copied to
 * @param	index	The index of the element
 * @return			Non-zero if an error has occured
 * 
 * @see		VecErr
 */
extern int v_at(Vec *vec, void *dest, size_t index);

/**
 * Insert an element at the specified index, shifting
 * all elements after it by one.
 *
 * @param	vec		Vector to be operated on
 * @param	elem	Pointer to the element to be inserted
 * @param	index	Index the element will be inserted at
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_insert(Vec *vec, void *elem, size_t index);
/**
 * Remove an element from the specified index, shifting
 * all elements after it by one.
 *
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the specified element will be copied to
 * @param	index	Index the element will be removed from
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_remove(Vec *vec, void *dest, size_t index);

/**
 * Insert an element at the specified index by replacing the current element
 * there and appending it to the end of the vector.
 *
 * @param	vec		Vector to be operated on
 * @param	elem	Pointer to element to be inserted
 * @param	index	Index the element will be inserted at
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_swap_insert(Vec *vec, void *elem, size_t index);
/**
 * Remove an element from the specified index by replacing it with the last
 * element of the vector.
 *
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer the specified element will be copied to
 * @param	index	Index the element will be removed from
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_swap_remove(Vec *vec, void *dest, size_t index);

/**
 * Raw data of a vector.
 *
 * @param	vec		Vector to be operated on
 * @return			Pointer to the vector's data, NULL if the
 *  				vector is empty or an error has occured
 *
 * @see		V_RAWNOCOPY
 */
extern void *v_raw(Vec *vec);
/**
 * Raw slice of a vector's data.
 *
 * @param	vec		Vector to be operated on
 * @param	from	Low bound index of the slice
 * @param	to		High bound index of the slice
 * @return			Pointer to the slice, NULL if the
 *  				slice is empty or an error has occured
 *
 * @see		V_RAWNOCOPY
 */
extern void *v_raw_slice(Vec *vec, size_t from, size_t to);

/**
 * Slice of a vector's data as a new vector.
 *
 * @param	vec		Vector to be operated on
 * @param	from	Low bound index of the slice
 * @param	to		High bound index of the slice
 * @return			Pointer to the slice vector, NULL if
 * 					an error has occured
 */
extern Vec *v_slice(Vec *vec, size_t from, size_t to);

/**
 * Prepend one or multiple elements to a vector.
 *
 * @param	vec		Vector to be operated on
 * @param	src		Pointer to the elements to be prepended
 * @param	amount	Amount of elements to be prepended
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_prepend(Vec *vec, void *src, size_t amount);
/**
 * Append one or multiple elements to a vector.
 *
 * @param	vec		Vector to be operated on
 * @param	src		Pointer to the elements to be appended
 * @param	amount	Amount of elements to be appended
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_append(Vec *vec, void *src, size_t amount);

/**
 * Trim multiple elements from the front of a vector.
 *
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer to a buffer the trimmed elements will be copied to
 * @param	amount	Amount of elements to be trimmed
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_trim_front(Vec *vec, void *dest, size_t amount);
/**
 * Trim multiple elements from the back of a vector.
 *
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer to a buffer the trimmed elements will be copied to
 * @param	amount	Amount of elements to be trimmed
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_trim_back(Vec *vec, void *dest, size_t amount);

/**
 * Insert multiple elements at the specified index, shifting
 * all elements after it by amount.
 *
 * @param	vec		Vector to be operated on
 * @param	elem	Pointer to the elements to be inserted
 * @param	index	Index the elements will be inserted at
 * @param	amount	Amount of elements to be inserted
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_insert_multiple(Vec *vec, void *src, size_t index, size_t amount);
/**
 * Remove multiple elements from the specified index, shifting
 * all elements after it by amount.
 *
 * @param	vec		Vector to be operated on
 * @param	dest	Pointer to a buffer the specified elements will be copied to
 * @param	index	Index the elements will be removed from
 * @param	amount	Amount of elements to be inserted
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_remove_multiple(Vec *vec, void *dest, size_t index, size_t amount);

/**
 * Split a vector into two vectors at a specified index
 *
 * @param	vec		Vector to be split, will retain lower half of the split on success
 * @param	index	Index the vector will be split at
 * @return			Higher part of the split vector, NULL if an error has occured
 */
extern Vec *v_split(Vec *vec, size_t index);

/**
 * Clone a vector.
 *
 * @param	vec		Vector to be operated on
 * @return			Cloned vector, NULL if an error has occured
 */
extern Vec *v_clone(Vec *vec);
/**
 * Zero all elements of a vector.
 *
 * @param	vec		Vector to be operated on
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_zero(Vec *vec);
/**
 * Clear a vector's elements while retaining its capacity.
 *
 * @param	vec		Vector to be operated on
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_softclear(Vec *vec);
/**
 * Clear a vector's elements and set its capacity to zero.
 *
 * @param	vec		Vector to be operated on
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_clear(Vec *vec);
/**
 * Destroy a vector.
 *
 * @param	vec		Vector to be destroyed
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int v_destroy(Vec *vec);

/**
 * Create an iterator over a vector.
 *
 * @param	vec		Vector to be iterated over
 * @return			Pointer to new iterator, NULL if an error has occured
 *
 * @see VecIter
 */
extern VecIter *v_iter(Vec *vec);
/**
 * Create an iterator over a vector, consuming the vector.
 *
 * @param	vec		Vector to be consumed
 * @return			Pointer to new iterator, NULL if an error has occured
 *
 * @see VecIter
 */
extern VecIter *v_into_iter(Vec **restrict vec);

/**
 * Check whether an iterator owns the vector it iterates over.
 *
 * @param	iter	Iterator to be operated on
 * @return			True if the specified iterator owns its vector, false if not or on error
 */
extern bool vi_is_owner(VecIter *iter);
/**
 * Returns the current position of the specified iterator.
 * 
 * @param	iter	Iterator to be operated on
 * @return			Current position of the iterator, 0 on error or if its position is 0
 */
extern size_t vi_pos(VecIter *iter);
/**
 * Check whether an iterator is done iterating over its vector.
 *
 * @param	iter	Iterator to be operated on
 * @return			True if the iterator is done iterating or on error, false if not
 */
extern bool vi_done(VecIter *iter);

/**
 * Return the element at the iterator's current position.
 *
 * @param	iter	Iterator to be operated on
 * @param	dest	Pointer the current element will be copied to
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_current(VecIter *iter, void *dest);
/**
 * Return the element at the iterator's current position and advance it by one.
 *
 * @param	iter	Iterator to be operated on
 * @param	dest	Pointer the current element will be copied to
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_next(VecIter *iter, void *dest);

/**
 * Skip amount elements of the iterator's vector.
 *
 * @param	iter	Iterator to be operated on
 * @param	amount	Amount of elements to be skipped
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_skip(VecIter *iter, size_t amount);
/**
 * Set the iterator to a specified index of its vector.
 *
 * @param	iter	Iterator to be operated on
 * @param	index	Index to be jumped to
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_goto(VecIter *iter, size_t index);
/**
 * Reset an iterator to the beginning of its vector.
 *
 * @param	iter	Iterator to be operated on
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_reset(VecIter *iter);

/**
 * Create a vector from an iterator, consuming the iterator.
 *
 * @param	iter	Iterator to be consumed
 * @return			Pointer to new vector, NULL if an error has occured
 */
extern Vec *vi_from_iter(VecIter *iter);
/**
 * Destroy an iterator.
 *
 * @param	iter	Iterator to be destroyed
 * @return			Non-zero if an error has occured
 *
 * @see		VecErr
 */
extern int vi_destroy(VecIter *iter);


#endif // C_VECTOR_H_
