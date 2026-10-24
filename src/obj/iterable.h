#ifndef LONG_ITERABLE_H
#define LONG_ITERABLE_H

#include <stdint.h>
#include "../common.h"
#include "../std/allocator.h"

typedef struct {
    const char* name;
    int64_t (*len)(const void* self);
    sv_opt_t(value_t) (*next)(void* self, const sv_allocator_t*);
    void (*deinit)(void* self, const sv_allocator_t*);
} iterable_vtable_t;

typedef struct {
    const iterable_vtable_t *vtable;
    void* self;
} iterable_t;

#define iterable_len(it) (it)->vtable->len((it)->self)
#define iterable_next(it, a) (it)->vtable->next((it)->self, a)
#define iterable_deinit(it, a) (it)->vtable->deinit((it)->self, a)

#endif
