#ifndef LONG_ACTOR_H
#define LONG_ACTOR_H

#include "../value.h"
#include "../common.h"
#include "../std/allocator.h"

typedef struct {
    const char* name;
    value_t update_fn;
    value_t get_fn;
} actor_vtable_t;

sv_vec_def(actor_vtable_t);

typedef struct actor_t {
    value_t state;
    const actor_vtable_t* vtable;
    bool busy;
} actor_t;

actor_t actor_init(value_t, const actor_vtable_t*);
void actor_free(actor_t*, const sv_allocator_t*);
value_t actor_get(value_t, value_t, vm_t*);
value_t actor_update(value_t, value_t, vm_t*);


#endif
