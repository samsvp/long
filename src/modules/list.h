#ifndef LONG_MOD_LIST
#define LONG_MOD_LIST

#include "../common.h"

/**
 * Returns a list where each element is the result of invoking `f` on each element of `seq`.
 */
value_t ntvl_map(const value_t*, uint8_t, vm_t*);
/**
 * Like `map`, but `f` also receives the element's index.
 */
value_t ntvl_mapi(const value_t*, uint8_t, vm_t*);
/**
 * Returns only the elements of `seq` for which `f` returns a truthy value.
 */
value_t ntvl_filter(const value_t*, uint8_t, vm_t*);
/**
 * Like `filter`, but `f` also receives the element's index.
 */
value_t ntvl_filteri(const value_t*, uint8_t, vm_t*);
/**
 * Invokes `f` for each element of `seq` with the accumulator, starting from `acc`.
 */
value_t ntvl_reduce(const value_t*, uint8_t, vm_t*);
/**
 * Like `reduce`, but `f` also receives the element's index.
 */
value_t ntvl_reducei(const value_t*, uint8_t, vm_t*);
/**
 * Zips corresponding elements of two sequences into a list of tuples, stopping at the shorter.
 */
value_t ntvl_zip(const value_t*, uint8_t, vm_t*);
/**
 * Zips corresponding elements of two sequences into a list, transforming them with `f`.
 */
value_t ntvl_zip_by(const value_t*, uint8_t, vm_t*);
/**
 * Returns true if `f` is truthy for at least one element of `seq`.
 */
value_t ntvl_any(const value_t*, uint8_t, vm_t*);
/**
 * Returns true if `f` is truthy for all elements of `seq`.
 */
value_t ntvl_all(const value_t*, uint8_t, vm_t*);
/**
 * Takes the first `n` elements of `seq`.
 */
value_t ntvl_take(const value_t*, uint8_t, vm_t*);
/**
 * Skips the first `n` elements of `seq`.
 */
value_t ntvl_skip(const value_t*, uint8_t, vm_t*);
/**
 * Finds the element at the zero-based `index`, counting from the end when negative. Returns
 * nil if `index` is out of bounds.
 */
value_t ntvl_at(const value_t*, uint8_t, vm_t*);
/**
 * Returns the number of elements in `seq`.
 */
value_t ntvl_count(const value_t*, uint8_t, vm_t*);
/**
 * Puts `x` in front of `seq`.
 */
value_t ntvl_prepend(const value_t*, uint8_t, vm_t*);
/**
 * Returns the minimal element of `seq`, or nil if it is empty.
 */
value_t ntvl_min(const value_t*, uint8_t, vm_t*);
/**
 * Returns the maximal element of `seq`, or nil if it is empty.
 */
value_t ntvl_max(const value_t*, uint8_t, vm_t*);
/**
 * Returns the minimal element of `seq` as calculated by `f`.
 */
value_t ntvl_min_by(const value_t*, uint8_t, vm_t*);
/**
 * Returns the maximal element of `seq` as calculated by `f`.
 */
value_t ntvl_max_by(const value_t*, uint8_t, vm_t*);
/**
 * Returns the index of the first element equal to `v`, or nil if there is none.
 */
value_t ntvl_find(const value_t*, uint8_t, vm_t*);
/**
 * Returns the first element for which `f` returns a truthy value, or nil if there is none.
 */
value_t ntvl_find_by(const value_t*, uint8_t, vm_t*);
/**
 * Invokes `f` for each element of `seq`. Returns nil.
 */
value_t ntvl_each(const value_t*, uint8_t, vm_t*);
/**
 * Concatenates `right` onto the end of `left`.
 */
value_t ntvl_concat(const value_t*, uint8_t, vm_t*);
/**
 * Sorts `seq` in natural order: numbers by value, strings bytewise.
 */
value_t ntvl_sort(const value_t*, uint8_t, vm_t*);
/**
 * Sorts `seq` by the results of `f`.
 */
value_t ntvl_sort_by(const value_t*, uint8_t, vm_t*);
/**
 * Returns the sum of all elements.
 */
value_t ntvl_sum(const value_t*, uint8_t, vm_t*);
/**
 * Returns the product of all elements.
 */
value_t ntvl_mul(const value_t*, uint8_t, vm_t*);

#endif
