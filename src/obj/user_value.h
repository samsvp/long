#ifndef LONG_USER_VALUE_H
#define LONG_USER_VALUE_H

#include "../common.h"
#include "../std/allocator.h"

typedef struct user_value_t {
    void* value;
    uint32_t tag;
    const char* name;
    const user_value_vtable_t* vtable;
} user_value_t;

bool uv_eql(user_value_t, user_value_t);
sv_opt_t(value_t) uv_field(user_value_t, uint32_t index, const vm_ctx_t*);
void uv_free(user_value_t, const sv_allocator_t*);
value_t uv_string(user_value_t, const vm_ctx_t*);
uint32_t uv_hash(user_value_t);

#endif
