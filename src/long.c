#include "long.h"
#include "compiler.h"
#include "std/allocator_std.h"

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
bool long_is_user(value_t v) { return IS_USER_VALUE(v); }

double long_as_number(value_t v) { return AS_NUMBER(v); }
bool long_as_bool(value_t v) { return AS_BOOL(v); }
void* long_as_c(value_t v) { return AS_C(v); }
const sv_str_t* long_as_str(value_t v) { return &AS_STR(v); }
const list_t* long_as_list(value_t v) { return &AS_LIST(v); }
const record_t* long_as_record(value_t v) { return &AS_RECORD(v); }
const tuple_t* long_as_tuple(value_t v) { return &AS_TUPLE(v); }
const hashmap_t* long_as_map(value_t v) { return &AS_MAP(v); }
const actor_t* long_as_actor(value_t v) { return &AS_ACTOR(v); }

void* long_as_user(value_t v, const user_value_vtable_t* vtable)
{
    if (!IS_USER_VALUE(v) || AS_USER_VALUE(v).vtable != vtable)
        return NULL;
    return AS_USER_VALUE(v).value;
}

value_t long_value_nil(void) { return value_nil; }
value_t long_value_oom(void) { return VALUE_OOM; }
bool long_is_oom(value_t v) { return IS_OBJ(v) && AS_OBJ(v).cell == NULL; }

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

value_t long_value_user(void* ptr, const user_value_vtable_t* vtable, uint32_t tag, const char* name,
                        const sv_allocator_t* a)
{
    value_t v = value_init_user_value(ptr, vtable, tag, name, a);
    if (long_is_oom(v))
        vtable->free_fn(ptr, a);
    return v;
}

void long_value_free(value_t* v, const sv_allocator_t* a) { value_free(v, a); }

value_t long_type_error(value_t arg, const char* expected, const vm_t* vm)
{
    return value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm_file(vm), vm_line(vm), vm->ctx.alloc,
                                    "Wrong type of argument: expected %s, got %s",
                                    expected, value_type_name(arg)),
                          vm->ctx.alloc);
}

const char* long_field_name(const vm_ctx_t* ctx, uint32_t index) { return vm_field_name(ctx, index); }
char* long_str_to_c(const sv_str_t* s, const sv_allocator_t* a) { return sv_str_to_c_str(*s, a); }

const sv_allocator_t* long_allocator(const vm_t* vm) { return vm->ctx.alloc; }
const sv_allocator_t* long_ctx_allocator(const vm_ctx_t* ctx) { return ctx->alloc; }
void* long_alloc(const sv_allocator_t* a, size_t size) { return sv_malloc(a, size); }
void long_free(const sv_allocator_t* a, void* p) { sv_free(a, p); }

int long_run_file(const char* path)
{
    ctx_t ctx = { .alloc = sv_gpa, .logger = sv_std_logger };
    vm_t vm = compile(path, COMPILER_DEFAULT_OPTS, &ctx);
    if (ctx.err.error_code != 0) {
        if (ctx.err.msg.size > 0)
            sv_log_error(&ctx.logger, "%.*s", (int)ctx.err.msg.size, ctx.err.msg.chars);
        error_free(&ctx.err, &ctx.alloc);
        return 1;
    }

    sv_opt_t(error_t) err = vm_run(&vm);
    if (err.is_some) {
        sv_log_error(&ctx.logger, "%.*s%.*s", (int)err.value.msg.size, err.value.msg.chars,
                     (int)err.value.trace.size, err.value.trace.chars);
        error_free(&err.value, &ctx.alloc);
        vm_deinit(&vm);
        return 1;
    }

    vm_deinit(&vm);
    return 0;
}
