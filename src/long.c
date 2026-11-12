#include "long.h"
#include "value.h"
#include "obj.h"
#include "obj/actor.h"

bool long_is_nil(value_t v) { return IS_NIL(v); }
bool long_is_number(value_t v) { return IS_NUMBER(v); }
bool long_is_bool(value_t v) { return IS_BOOL(v); }
bool long_is_c(value_t v) { return IS_C(v); }
bool long_is_str(value_t v) { return IS_STR(v); }
bool long_is_list(value_t v) { return IS_LIST(v); }
bool long_is_record(value_t v) { return IS_RECORD(v); }
bool long_is_tuple(value_t v) { return IS_TUPLE(v); }
bool long_is_map(value_t v) { return IS_MAP(v); }
bool long_is_atom(value_t v) { return IS_ATOM(v); }
bool long_is_actor(value_t v) { return IS_ACTOR(v); }
bool long_is_falsey(value_t v) { return IS_FALSEY(v); }
bool long_is_function(value_t v) { return IS_NATIVE(v) || IS_CLOSURE(v) || IS_CLOSURE_MEMBER(v); }

double long_as_number(value_t v) { return AS_NUMBER(v); }
bool long_as_bool(value_t v) { return AS_BOOL(v); }
void* long_as_c(value_t v) { return AS_C(v); }
const sv_str_t* long_as_str(value_t v) { return &AS_STR(v); }
const list_t* long_as_list(value_t v) { return &AS_LIST(v); }
const record_t* long_as_record(value_t v) { return &AS_RECORD(v); }
const tuple_t* long_as_tuple(value_t v) { return &AS_TUPLE(v); }
const hashmap_t* long_as_map(value_t v) { return &AS_MAP(v); }
const actor_t* long_as_actor(value_t v) { return &AS_ACTOR(v); }

value_t long_value_number(double n) { return value_number(n); }
value_t long_value_bool(bool b) { return value_bool(b); }
value_t long_value_c(void* p) { return value_c(p); }

value_t long_value_str(const char* s, const sv_allocator_t* a)
{
    return value_init_str(sv_str_init(s), a);
}

value_t long_value_list(const value_t* values, int64_t n, const sv_allocator_t* a)
{
    return value_init_list(values, n, a);
}

value_t long_value_record(const value_t* pairs, uint8_t n, const sv_allocator_t* a)
{
    return value_init_record(pairs, n, a);
}

value_t long_value_tuple(const value_t* values, uint8_t n, const sv_allocator_t* a)
{
    return value_init_tuple(values, n, a);
}

value_t long_value_map(const value_t* pairs, int64_t n, const sv_allocator_t* a)
{
    return value_init_map(pairs, n, a);
}

value_t long_value_actor(value_t state, const actor_vtable_t* vtable, const sv_allocator_t* a)
{
    actor_t actor = actor_init(state, vtable);
    return value_wrap_actor(&actor, a);
}
