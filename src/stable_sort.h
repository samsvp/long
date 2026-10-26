#ifndef LONG_STABLE_SORT_H
#define LONG_STABLE_SORT_H

#include <stddef.h>

/**
 * Stable sort with the qsort interface. Small arrays are insertion-sorted in
 * place; larger ones use a bottom-up mergesort over insertion-sorted blocks.
 * If the temporary buffer allocation fails, it falls back to a
 * full in-place insertion sort.
 */
void stable_sort(void*, size_t, size_t, void* ctx, int (*)(void* ctx, const void*, const void*));

#endif
