#ifndef LONG_ITERATOR_H
#define LONG_ITERATOR_H

#include "../std/allocator.h"
#include "iterable.h"

/**
 * Initializes an iterator from an iterable value. self is NULL on allocation failure.
 */
iterable_t iter_init(value_t, const sv_allocator_t*);
/**
 * Deinitializes the iterator.
 */
void iter_deinit(iterable_t*, const sv_allocator_t*);

#endif
