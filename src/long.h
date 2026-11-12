#ifndef LONG_H
#define LONG_H

/**
 * Public API for C modules.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VALUE_NUMBER,
    VALUE_ATOM,
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_C,
    VALUE_UNDEFINED,
    VALUE_OBJ,
} value_kind;

// Types
typedef struct sv_allocator_t sv_allocator_t;

typedef struct cell_obj_t cell_obj_t;
typedef struct rc_obj_t_t { cell_obj_t* cell; } rc_obj_t_t;

typedef struct sv_str_t sv_str_t;
typedef struct record_t record_t;
typedef struct tuple_t tuple_t;
typedef struct actor_t actor_t;
typedef struct actor_vtable_t actor_vtable_t;
// ref cell types
typedef struct rc_node_t_t list_t;
typedef struct rc_map_node_t_t hashmap_t;

#if defined(_WIN32)
  #ifdef LONG_BUILD
    #define LONG_API __declspec(dllexport)
  #else
    #define LONG_API __declspec(dllimport)
  #endif
#else
  #define LONG_API __attribute__((visibility("default")))
#endif

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

#else

typedef struct value_t {
    value_kind kind;
    union {
        double number;
        bool boolean;
        void* c;
        rc_obj_t_t obj;
    };
} value_t;

#endif

LONG_API bool long_is_nil(value_t);
LONG_API bool long_is_number(value_t);
LONG_API bool long_is_bool(value_t);
LONG_API bool long_is_c(value_t);
LONG_API bool long_is_str(value_t);
LONG_API bool long_is_list(value_t);
LONG_API bool long_is_record(value_t);
LONG_API bool long_is_tuple(value_t);
LONG_API bool long_is_map(value_t);
LONG_API bool long_is_atom(value_t);
LONG_API bool long_is_actor(value_t);
LONG_API bool long_is_falsey(value_t);
LONG_API bool long_is_function(value_t);

LONG_API double long_as_number(value_t);
LONG_API bool long_as_bool(value_t);
LONG_API void* long_as_c(value_t);
LONG_API const sv_str_t* long_as_str(value_t);
LONG_API const list_t* long_as_list(value_t);
LONG_API const record_t* long_as_record(value_t);
LONG_API const tuple_t* long_as_tuple(value_t);
LONG_API const hashmap_t* long_as_map(value_t);
LONG_API const actor_t* long_as_actor(value_t);

LONG_API value_t long_value_number(double);
LONG_API value_t long_value_bool(bool);
LONG_API value_t long_value_c(void*);
LONG_API value_t long_value_str(const char*, const sv_allocator_t*);
LONG_API value_t long_value_list(const value_t*, int64_t, const sv_allocator_t*);
// Initializes the record with the given value array. The value array must contain the id as a double value and
// its corresponding value. The size parameter must be equal to the array length / 2. The pairs must be sorted
// ascending by id: lookups binary-search and equality compares positionally.
LONG_API value_t long_value_record(const value_t*, uint8_t, const sv_allocator_t*);
LONG_API value_t long_value_tuple(const value_t*, uint8_t, const sv_allocator_t*);
// Initializes a new map from the (key, values) pair array.
LONG_API value_t long_value_map(const value_t*, int64_t, const sv_allocator_t*);
LONG_API value_t long_value_actor(value_t, const actor_vtable_t*, const sv_allocator_t*);

#endif
