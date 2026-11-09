#ifndef LONG_OBJ_H
#define LONG_OBJ_H

#include "obj/actor.h"
#include "obj/user_value.h"
#include "std/string.h"
#include "common.h"
#include "value.h"
#include "error.h"
#include "obj/closure.h"
#include "obj/native_fns.h"
#include "obj/record.h"
#include "obj/tuple.h"

typedef enum {
    OBJ_STR,
    OBJ_LIST,
    OBJ_RECORD,
    OBJ_TUPLE,
    OBJ_MAP,
    OBJ_ITER,
    OBJ_ACTOR,
    OBJ_NATIVE_FN,
    OBJ_CLOSURE,
    OBJ_CLOSURE_MEMBER,
    OBJ_USER_VALUE,
    OBJ_ERR,
} obj_kind;

const char* value_kind_str(value_kind, obj_kind);

typedef struct obj_t {
    union {
        sv_str_t str;
        record_t record;
        tuple_t tuple;
        list_t list;
        hashmap_t map;
        iterable_t iter;
        native_fn_t fn;
        closure_t closure;
        closure_member_t closure_member;
        user_value_t uservalue;
        actor_t actor;
        error_t err;
    };
    obj_kind kind;
} obj_t;

sv_rc_cell_def(obj_t);

#define VALUE_OOM value_obj((sv_rc_t(obj_t)){ 0 })

static inline void value_free(value_t* v, const sv_allocator_t* a)
{
    if (IS_OBJ(*v)) {
        sv_rc_t(obj_t) rc = AS_OBJ(*v);
        sv_rc_deinit(&rc, a);
        *v = value_obj(rc);
    }
}

static inline value_t value_borrow(value_t v)
{
    if (IS_OBJ(v))
        (void)sv_rc_borrow(AS_OBJ(v));
    return v;
}

#endif
