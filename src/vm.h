#ifndef LONG_VM_H
#define LONG_VM_H

#include <stdint.h>
#include "std/logger.h"
#include "value.h"
#include "obj.h"
#include "obj/fn.h"
#include "common.h"
#include "std/vector.h"
#include "globals.h"
#include "native_lib.h"

typedef struct vm_t vm_t;

typedef sv_vec_t(value_t) value_arr;

/**
 * A VM context to be passed to native functions.
 */
typedef struct vm_ctx_t {
    const sv_allocator_t* alloc;
    sv_logger_t logger;

    hashmap_t record_fields;
    const char** record_key_names;
    uint32_t record_names_sizes;
    const char** atom_names;
    uint32_t atom_names_size;
} vm_ctx_t;

typedef struct {
    fn_t* fn;
    int64_t ip;

    int64_t locals_offset;
    int64_t stack_offset;

    sv_rc_t(closure_group_t) group;
    value_arr upvalues;
} call_frame_t;

sv_vec_def(call_frame_t);

typedef struct vm_t {
    sv_vec_t(call_frame_t) call_frames;
    int64_t max_call_frames;
    int64_t native_depth;

    globals_t globals_names_to_index;

    fn_t fn;

    value_arr globals;
    value_arr locals;
    value_arr stack;

    sv_vec_t(actor_vtable_t) actor_vtables;
    sv_vec_t(sv_str_t) files;
    sv_vec_t(native_lib_t) native_libs;

    vm_ctx_t ctx;
} vm_t;

/**
 * Creates a vm that owns and runs `fn`.
 */
vm_t vm_init(fn_t, int64_t max_call_frames, globals_t globals_names_to_index, const sv_allocator_t*);
void vm_deinit(vm_t*);

/**
 * Returns the compiled index of the given record name.
 */
sv_opt_t(uint32_t) vm_get_record_idx(vm_t, sv_str_t);
/**
 * The source name of a record field, for messages and printing. A vm built by hand has no
 * table, and shows the field as `?`.
 */
const char* vm_field_name(const vm_ctx_t*, uint32_t id);
/**
 * Returns the line of the instruction being executed, 0 outside one.
 */
int64_t vm_line(const vm_t*);
/**
 * Returns the file of the instruction being executed, NULL outside one.
 */
const char* vm_file(const vm_t*);

/**
 * Interprets the VM bytecode.
 */
sv_opt_t(error_t) vm_run(vm_t*);
/**
 * Calls a callable value on top of the vm.
 */
value_t vm_call_value(vm_t*, value_t, const value_t*, uint8_t);
/**
 * Calls the given function by name.
 */
value_t vm_call_name(vm_t*, value_t*, uint8_t, sv_str_t function_name, sv_str_t file_name);

typedef enum {
#define OP_CODE(op) op,
#include "vm_instructions.h"
#undef OP_CODE
} vm_instructions;

typedef enum {
    VM_ERR_OOM,
    VM_ERR_UNDEFINED_VARIABLE,
    VM_ERR_OP_UNSUPPORTED_ARGS,
    VM_ERR_NOT_IMPLEMENTED,
    VM_ERR_NO_GROUP,
    VM_ERR_BAD_ARITY,
    VM_ERR_KEY_NOT_FOUND,
    VM_ERR_FIELD_NOT_FOUND,
    VM_ERR_WRONG_TYPE,
    VM_ERR_MATCH_FAILED,
    VM_ERR_NO_CLAUSE,
    VM_ERR_STACK_OVERFLOW,
} vm_error_kinds;

#endif
