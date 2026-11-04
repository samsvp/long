#ifndef LONG_ITERATOR_H
#define LONG_ITERATOR_H

#include "../std/allocator.h"
#include "iterable.h"
#include "list.h"
#include "map.h"
#include "str.h"
#include "tuple.h"

/**
 * Storage for the state of any iterator, so an iterator can live on the stack.
 */
typedef union {
    ll_iter_t list;
    str_iter_t str;
    map_iter_t map;
    tuple_iter_t tuple;
} iter_storage_t;

/**
 * Initializes an iterator from an iterable value. self is NULL on allocation failure.
 */
iterable_t iter_init(value_t, const sv_allocator_t*);
/**
 * Initializes an iterator viewing an iterable value, without borrowing it nor
 * allocating: the iterator state lives in the given storage. Both the value and
 * the storage must outlive the iterator, and no deinit is needed; never call
 * iter_deinit on it. Values returned by iterable_next are still owned by the
 * caller and must be freed. self is NULL when the value is not iterable.
 */
iterable_t iter_init_no_borrow(value_t, iter_storage_t*);
/**
 * Deinitializes an iterator created with iter_init.
 */
void iter_deinit(iterable_t*, const sv_allocator_t*);

#endif
