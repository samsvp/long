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
 * add(list1, list2): concatenates list1 and list2.
 */
value_t ntvl_add(const value_t*, uint8_t, vm_t*);
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
/**
 * combinations(lst, k): returns every combination of k elements of lst, keeping their order.
 */
value_t ntvl_combinations(const value_t*, uint8_t, vm_t*);
/**
 * map_window(lst, f, window_len): applies f to successive windows of lst.
 */
value_t ntvl_map_window(const value_t*, uint8_t, vm_t*);
/**
 * mapf_window(lst, f, window_len): applies f to successive windows of lst, filtering falsey results.
 */
value_t ntvl_mapf_window(const value_t*, uint8_t, vm_t*);
/**
 * filter_window(lst, f, window_len): filters the windows which returns a falsey result.
 */
value_t ntvl_filter_window(const value_t*, uint8_t, vm_t*);
/**
 * sort(lst): returns a sorted list.
 */
value_t ntvl_sort(const value_t*, uint8_t, vm_t*);
/**
 * sort_by(lst, f): returns a sorted list applying f as a comparison function.
 */
value_t ntvl_sort_by(const value_t*, uint8_t, vm_t*);
/**
 * all(lst, f): returns true if the all elements of the list satisfies f.
 */
value_t ntvl_all(const value_t*, uint8_t, vm_t*);
/**
 * any(lst, f): returns true if the any element of the list satisfies f.
 */
value_t ntvl_any(const value_t*, uint8_t, vm_t*);
/**
 * max(lst): returns the maximum element of lst.
 */
value_t ntvl_max(const value_t*, uint8_t, vm_t*);
/**
 * max_by(lst, f): returns the element of lst where f(elem) is the maximum.
 */
value_t ntvl_max_by(const value_t*, uint8_t, vm_t*);
/**
 * min(lst): returns the minimum element of lst.
 */
value_t ntvl_min(const value_t*, uint8_t, vm_t*);
/**
 * min_by(lst, f): returns the element of lst where f(elem) is the minimum.
 */
value_t ntvl_min_by(const value_t*, uint8_t, vm_t*);
/**
 * rev(lst): returns the reversed list.
 */
value_t ntvl_rev(const value_t*, uint8_t, vm_t*);
/**
 * count(lst, val): returns the amount of times val appears in lst.
 */
value_t ntvl_count(const value_t*, uint8_t, vm_t*);
/**
 * count_by(lst, f): maps f to each element of lst and counts the amount of times f(x) is truthy.
 */
value_t ntvl_count_by(const value_t*, uint8_t, vm_t*);

#endif
