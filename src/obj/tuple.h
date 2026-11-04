#ifndef LONG_TUPLE_H
#define LONG_TUPLE_H

#include <stdint.h>
#include "../common.h"
#include "../std/allocator.h"
#include "../std/option.h"
#include "iterable.h"

typedef struct tuple_t {
    value_t* items;
    uint8_t size;
} tuple_t;

/**
 * Initializes the tuple borrowing the values. items is NULL on allocation
 * failure. The size must be at least 1: the syntax has no empty tuple.
 */
tuple_t tuple_init(const value_t*, uint8_t, const sv_allocator_t*);
/**
 * Deinits the tuple values and its value array.
 */
void tuple_deinit(tuple_t*, const sv_allocator_t*);
/**
 * Checks if two tuples are equal.
 */
bool tuple_eql(tuple_t, tuple_t);
/**
 * Returns the element at the index, counting from the end when negative, none when out of range.
 */
sv_opt_t(value_t) tuple_get(tuple_t, int64_t);

typedef struct {
    sv_rc_t(obj_t) root;
    tuple_t tuple;
    uint8_t i;
} tuple_iter_t;

/**
 * Initializes an iterator over the tuple value, borrowing it.
 */
tuple_iter_t tuple_iter_init(value_t);
/**
 * Initializes an iterator viewing the tuple value without borrowing it. The
 * tuple must outlive the iterator; no deinit is needed.
 */
tuple_iter_t tuple_iter_init_no_borrow(value_t);
/**
 * Deinitializes the iterator, releasing the tuple if it was borrowed.
 */
void tuple_iter_deinit(tuple_iter_t*, const sv_allocator_t*);
iterable_t tuple_iterable_init(tuple_iter_t*);

#endif
