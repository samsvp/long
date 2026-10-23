#ifndef LONG_MOD_LIST
#define LONG_MOD_LIST

#include "../common.h"

/**
 * map(list, f): apply f on each element of the list and returns a new list.
 */
value_t ntvl_map(const value_t*, uint8_t, vm_t*);
/**
 * map(list, f): apply f(x, index) on each element of the list and returns a new list.
 */
value_t ntvl_imap(const value_t*, uint8_t, vm_t*);
/**
 * filter(list, f): returns a new list with the elements f(x) is truthy.
 */
value_t ntvl_filter(const value_t*, uint8_t, vm_t*);
/**
 * filter(list, f): returns a new list with the elements f(x, index) is truthy.
 */
value_t ntvl_ifilter(const value_t*, uint8_t, vm_t*);
/**
 * reduce(list, f, acc): applies f(x, acc) updating acc with the return value of the function.
 */
value_t ntvl_reduce(const value_t*, uint8_t, vm_t*);
/**
 * reduce(list, f, acc): applies f(x, acc, index) updating acc with the return value of the function.
 */
value_t ntvl_ireduce(const value_t*, uint8_t, vm_t*);
/**
 * zip(lst1, lst2): zips two lists into a tuple of every element. Both must have the same length.
 */
value_t ntvl_zip(const value_t*, uint8_t, vm_t*);
/**
 * shuffle(lst): returns a shuffled array.
 */
value_t ntvl_shuffle(const value_t*, uint8_t, vm_t*);
/**
 * take(lst, n): takes the first n elements from n.
 */
value_t ntvl_take(const value_t*, uint8_t, vm_t*);
/**
 * skip(lst, n): skips the first n elements from n.
 */
value_t ntvl_skip(const value_t*, uint8_t, vm_t*);

#endif
