#ifndef LONG_VALUE_H
#define LONG_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "std/rc.h"
#include "std/option.h"
#include "std/string.h"
#include "std/allocator.h"
#include "obj/native_fns.h"
#include "obj/closure.h"
#include "obj/list.h"
#include "error.h"
#include "obj/user_value.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_ATOM,
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_C,
    VALUE_UNDEFINED,
    VALUE_OBJ,
} value_kind;

#ifndef LONG_NAN_BOXING
  #if UINTPTR_MAX == UINT64_MAX
    #define LONG_NAN_BOXING 1
  #else
    #define LONG_NAN_BOXING 0
  #endif
#endif

#if LONG_NAN_BOXING

typedef struct value_t {
    uint64_t bits;
} value_t;

#define VALUE_QNAN ((uint64_t)0x7ffc000000000000)
#define VALUE_SIGN ((uint64_t)0x8000000000000000)
#define VALUE_TAG_MASK ((uint64_t)3 << 48)
#define VALUE_TAG_ATOM ((uint64_t)1 << 48)
#define VALUE_TAG_C ((uint64_t)2 << 48)
#define VALUE_TAG_SINGLE ((uint64_t)3 << 48)
#define VALUE_PAYLOAD ((uint64_t)0x0000ffffffffffff)
#define VALUE_NIL_BITS (VALUE_QNAN | VALUE_TAG_SINGLE | 1)
#define VALUE_FALSE_BITS (VALUE_QNAN | VALUE_TAG_SINGLE | 2)
#define VALUE_TRUE_BITS (VALUE_QNAN | VALUE_TAG_SINGLE | 3)
#define VALUE_UNDEFINED_BITS (VALUE_QNAN | VALUE_TAG_SINGLE | 4)

static inline value_kind value_kind_of(value_t v)
{
    if ((v.bits & VALUE_QNAN) != VALUE_QNAN)
        return VALUE_NUMBER;
    if (v.bits & VALUE_SIGN)
        return VALUE_OBJ;
    switch (v.bits & VALUE_TAG_MASK) {
        case VALUE_TAG_ATOM: return VALUE_ATOM;
        case VALUE_TAG_C: return VALUE_C;
        default: break;
    }
    if (v.bits == VALUE_NIL_BITS)
        return VALUE_NIL;
    if (v.bits == VALUE_UNDEFINED_BITS)
        return VALUE_UNDEFINED;
    return VALUE_BOOL;
}

static inline double value_as_number(value_t v) { double d; memcpy(&d, &v.bits, sizeof(d)); return d; }
static inline value_t value_number(double n) { value_t v; memcpy(&v.bits, &n, sizeof(n)); return v; }

#define VALUE_KIND(v) value_kind_of(v)
#define IS_OBJ(v) (((v).bits & (VALUE_QNAN | VALUE_SIGN)) == (VALUE_QNAN | VALUE_SIGN))
#define IS_NUMBER(v) (((v).bits & VALUE_QNAN) != VALUE_QNAN)
#define IS_ATOM(v) (((v).bits & (VALUE_QNAN | VALUE_SIGN | VALUE_TAG_MASK)) == (VALUE_QNAN | VALUE_TAG_ATOM))
#define IS_C(v) (((v).bits & (VALUE_QNAN | VALUE_SIGN | VALUE_TAG_MASK)) == (VALUE_QNAN | VALUE_TAG_C))
#define IS_NIL(v) ((v).bits == VALUE_NIL_BITS)
#define IS_BOOL(v) (((v).bits | 1) == VALUE_TRUE_BITS)
#define AS_OBJ(v) ((sv_rc_t(obj_t)){ .cell = (void*)(uintptr_t)((v).bits & VALUE_PAYLOAD) })
#define AS_NUMBER(v) value_as_number(v)
#define AS_ATOM(v) ((double)(uint32_t)((v).bits & VALUE_PAYLOAD))
#define AS_BOOL(v) ((v).bits == VALUE_TRUE_BITS)
#define AS_C(v) ((void*)(uintptr_t)((v).bits & VALUE_PAYLOAD))

#define value_atom(id) ((value_t){ VALUE_QNAN | VALUE_TAG_ATOM | (uint32_t)(id) })
#define value_bool(b) ((value_t){ (b) ? VALUE_TRUE_BITS : VALUE_FALSE_BITS })
#define value_c(p) ((value_t){ VALUE_QNAN | VALUE_TAG_C | ((uintptr_t)(p) & VALUE_PAYLOAD) })
#define value_obj(o) ((value_t){ VALUE_QNAN | VALUE_SIGN | ((uintptr_t)(o).cell & VALUE_PAYLOAD) })

static const value_t value_nil = { VALUE_NIL_BITS };
static const value_t value_undefined = { VALUE_UNDEFINED_BITS };
static const value_t value_true = { VALUE_TRUE_BITS };
static const value_t value_false = { VALUE_FALSE_BITS };

#else

typedef struct value_t {
    value_kind kind;
    union {
        double number;
        bool boolean;
        void* c;
        sv_rc_t(obj_t) obj;
    };
} value_t;

#define VALUE_KIND(v) ((v).kind)
#define IS_OBJ(v) ((v).kind == VALUE_OBJ)
#define IS_NUMBER(v) ((v).kind == VALUE_NUMBER)
#define IS_ATOM(v) ((v).kind == VALUE_ATOM)
#define IS_C(v) ((v).kind == VALUE_C)
#define IS_NIL(v) ((v).kind == VALUE_NIL)
#define IS_BOOL(v) ((v).kind == VALUE_BOOL)
#define AS_OBJ(v) ((v).obj)
#define AS_NUMBER(v) ((v).number)
#define AS_ATOM(v) ((v).number)
#define AS_BOOL(v) ((v).boolean)
#define AS_C(v) ((v).c)

#define value_number(n) ((value_t){ .kind = VALUE_NUMBER, .number = (n) })
#define value_atom(id) ((value_t){ .kind = VALUE_ATOM, .number = (id) })
#define value_bool(b) ((value_t){ .kind = VALUE_BOOL, .boolean = (b) })
#define value_c(p) ((value_t){ .kind = VALUE_C, .c = (p) })
#define value_obj(o) ((value_t){ .kind = VALUE_OBJ, .obj = (o) })

static const value_t value_nil = { .kind = VALUE_NIL };
static const value_t value_undefined = { .kind = VALUE_UNDEFINED };
static const value_t value_true = { .kind = VALUE_BOOL, .boolean = true };
static const value_t value_false = { .kind = VALUE_BOOL, .boolean = false };

#endif

#define IS_FALSEY(v) (IS_NIL(v) || ( IS_BOOL((v)) && !AS_BOOL((v)) ) )
#define IS_OBJ_KIND(v, k) (IS_OBJ(v) && AS_OBJ(v).cell->value.kind == (k))
#define IS_STR(v) IS_OBJ_KIND(v, OBJ_STR)
#define IS_ACTOR(v) IS_OBJ_KIND(v, OBJ_ACTOR)
#define IS_ERR(v) IS_OBJ_KIND(v, OBJ_ERR)
#define IS_LIST(v) IS_OBJ_KIND(v, OBJ_LIST)
#define IS_CONS(v) (IS_LIST(v) && !ll_is_empty(AS_LIST(v)))
#define IS_NATIVE(v) IS_OBJ_KIND(v, OBJ_NATIVE_FN)
#define IS_CLOSURE(v) IS_OBJ_KIND(v, OBJ_CLOSURE)
#define IS_CLOSURE_MEMBER(v) IS_OBJ_KIND(v, OBJ_CLOSURE_MEMBER)
#define IS_ITER(v) IS_OBJ_KIND(v, OBJ_ITER)
#define IS_RECORD(v) IS_OBJ_KIND(v, OBJ_RECORD)
#define IS_TUPLE(v) IS_OBJ_KIND(v, OBJ_TUPLE)
#define IS_MAP(v) IS_OBJ_KIND(v, OBJ_MAP)
#define IS_USER_VALUE(v) IS_OBJ_KIND(v, OBJ_USER_VALUE)

#define AS_STR(v) (AS_OBJ(v).cell->value.str)
#define AS_ACTOR(v) (AS_OBJ(v).cell->value.actor)
#define AS_ERR(v) (AS_OBJ(v).cell->value.err)
#define AS_LIST(v) (AS_OBJ(v).cell->value.list)
#define AS_NATIVE(v) (AS_OBJ(v).cell->value.fn)
#define AS_CLOSURE(v) (AS_OBJ(v).cell->value.closure)
#define AS_CLOSURE_MEMBER(v) (AS_OBJ(v).cell->value.closure_member)
#define AS_ITER(v) (AS_OBJ(v).cell->value.iter)
#define AS_RECORD(v) (AS_OBJ(v).cell->value.record)
#define AS_TUPLE(v) (AS_OBJ(v).cell->value.tuple)
#define AS_MAP(v) (AS_OBJ(v).cell->value.map)
#define AS_USER_VALUE(v) (AS_OBJ(v).cell->value.uservalue)

sv_opt_def(value_t);

/**
 * Frees every value in the vector and the vector itself.
 */
void value_arr_deinit(sv_vec_t(value_t)*, const sv_allocator_t*);
/**
 * Only nil, false and undefined are falsy.
 */
bool value_is_truthy(value_t);
bool value_eql(value_t, value_t);

/**
 * Converts the value a string representation.
 */
sv_str_t value_to_str(value_t, const vm_ctx_t*);
/**
 * Appends the value's text to the builder. False when it cannot be allocated.
 */
bool value_write(value_t, sv_str_builder*, const vm_ctx_t*);
/**
 * The type of a value as an error message names it: number, string, list, ...
 */
const char* value_type_name(value_t);

/**
 * Creates a list value cloning the values (see ll_init). obj.cell is NULL on
 * allocation failure.
 */
value_t value_init_list(const value_t*, int64_t, const sv_allocator_t*);
value_t value_init_list_filter(const value_t*, int64_t, const sv_allocator_t*);
value_t value_wrap_list(list_t, const sv_allocator_t*);
/**
 * Wraps an already built record or map into a value, taking ownership.
 */
value_t value_wrap_record(record_t, const sv_allocator_t*);
value_t value_wrap_map(hashmap_t, const sv_allocator_t*);
/**
 * Wraps an actor into a value.
 */
value_t value_wrap_actor(actor_t*, const sv_allocator_t*);
/**
 * Creates an iterator value borrowing the given list value. The value must
 * hold an iterable object. obj.cell is NULL on allocation failure.
 */
value_t value_init_iter(value_t, const sv_allocator_t*);
/**
 * Creates a closure value over the function, borrowing the upvalues.
 * obj.cell is NULL on allocation failure.
 */
value_t value_init_closure(fn_t*, const value_t*, int64_t, const sv_allocator_t*);

/**
 * Creates a new error value.
 */
value_t value_init_err(error_t, const sv_allocator_t*);

/**
 * Creates a new native function.
 */
value_t value_init_native(native_fn_t, const sv_allocator_t*);

/**
 * Creates a closure member value taking ownership of the group reference.
 * obj.cell is NULL on allocation failure.
 */
value_t value_init_closure_member(sv_rc_t(closure_group_t), int64_t, const sv_allocator_t*);
/**
 * Creates a string value copying the given string. obj.cell is NULL on
 * allocation failure.
 */
value_t value_init_str(sv_str_t, const sv_allocator_t*);
/**
 * Creates a string value taking ownership of the given string. obj.cell is
 * NULL on allocation failure (the string is freed).
 */
value_t value_init_str_own(sv_str_t, const sv_allocator_t*);
/**
 * Creates a map value borrowing n_pairs (key, value) pairs from the flat
 * array. Later duplicate keys win. obj.cell is NULL on allocation failure.
 */
value_t value_init_map(const value_t*, int64_t, const sv_allocator_t*);
/**
 * Creates a tuple value borrowing the values. The value array order must be (id, value).
 * obj.cell is NULL on allocation failure.
 */
value_t value_init_record(const value_t*, uint8_t, const sv_allocator_t*);
/**
 * Creates a tuple value borrowing the values. obj.cell is NULL on allocation
 * failure.
 */
value_t value_init_tuple(const value_t*, uint8_t, const sv_allocator_t*);
/**
 * Creates a new user value.
 */
value_t value_init_user_value(void*, const user_value_vtable_t*,
                              uint32_t tag, const char* name, const sv_allocator_t*);

#endif
