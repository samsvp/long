#ifndef LONG_H
#define LONG_H

/**
 * Public API for native modules and embedders.
 */

#include <stdbool.h>
#include <stddef.h>
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
typedef struct vm_t vm_t;
typedef struct vm_ctx_t vm_ctx_t;
// ref cell types
typedef struct rc_node_t_t list_t;
typedef struct rc_map_node_t_t hashmap_t;

#if defined(_WIN32)
  #ifdef LONG_BUILD
    #define LONG_API __declspec(dllexport)
  #else
    #define LONG_API __declspec(dllimport)
  #endif
  #define LONG_MODULE_API __declspec(dllexport)
#else
  #define LONG_API __attribute__((visibility("default")))
  #define LONG_MODULE_API __attribute__((visibility("default")))
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

typedef struct user_value_vtable_t {
    // true if two values are equal
    bool (*eql_fn)(void*, void*);
    // free the value
    void (*free_fn)(void*, const sv_allocator_t*);
    // writes the field to out as a borrowed value, false if there is no such field
    bool (*get_field_fn)(void*, uint32_t index, value_t* out, const vm_ctx_t*);
    // string value for printing
    value_t (*to_string_fn)(void*, const vm_ctx_t*);
    // hash function for hashmaps
    uint32_t (*hash_fn)(void*);
} user_value_vtable_t;

typedef struct native_fn_t {
    value_t (*fn) (const value_t*, uint8_t, vm_t*);
    uint8_t arity;
    const char* name;
} native_fn_t;

typedef struct native_module_t {
    const char* name;
    native_fn_t* native_fns;
    int native_count;
} native_module_t;

/**
 * Entry point every native module library defines. Returns the module's functions.
 */
LONG_MODULE_API const native_module_t* long_module_init(void);

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
LONG_API bool long_is_user(value_t);

LONG_API double long_as_number(value_t);
LONG_API bool long_as_bool(value_t);
LONG_API void* long_as_c(value_t);
LONG_API const sv_str_t* long_as_str(value_t);
LONG_API const list_t* long_as_list(value_t);
LONG_API const record_t* long_as_record(value_t);
LONG_API const tuple_t* long_as_tuple(value_t);
LONG_API const hashmap_t* long_as_map(value_t);
LONG_API const actor_t* long_as_actor(value_t);
/**
 * The user value's pointer if it was created with the vtable, NULL otherwise.
 */
LONG_API void* long_as_user(value_t, const user_value_vtable_t*);

LONG_API value_t long_value_nil(void);
LONG_API value_t long_value_oom(void);
LONG_API bool long_is_oom(value_t);

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
/**
 * Wraps the pointer in a user value. On failure, frees the pointer with the vtable's free_fn.
 */
LONG_API value_t long_value_user(void*, const user_value_vtable_t*, uint32_t tag, const char* name,
                                 const sv_allocator_t*);
LONG_API void long_value_free(value_t*, const sv_allocator_t*);
/**
 * The error a native returns when an argument is not of the expected type.
 */
LONG_API value_t long_type_error(value_t arg, const char* expected, const vm_t*);

/**
 * The source name of the field with the index, for get_field_fn.
 */
LONG_API const char* long_field_name(const vm_ctx_t*, uint32_t index);
/**
 * A NUL-terminated copy of the string, freed with long_free.
 */
LONG_API char* long_str_to_c(const sv_str_t*, const sv_allocator_t*);

/**
 * The allocator the VM allocates and frees values with.
 */
LONG_API const sv_allocator_t* long_allocator(const vm_t*);
LONG_API const sv_allocator_t* long_ctx_allocator(const vm_ctx_t*);
LONG_API void* long_alloc(const sv_allocator_t*, size_t);
LONG_API void long_free(const sv_allocator_t*, void*);

/**
 * Compiles and runs the file, logging errors to stderr. Returns 0 on success, 1 on failure.
 */
LONG_API int long_run_file(const char* path);

#endif
