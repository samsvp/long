#include "std_native.h"

#include "obj/iterator.h"
#include "obj/iterable.h"
#include "value.h"
#include "std/allocator_std.h"
#include "obj/list.h"
#include "obj/str.h"
#include "vm.h"
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#ifndef RAND_SEED_DEFINED
    static int initialized = false;
#else
    static int initialized = true;
#endif

void init_rand(void)
{
    if (!initialized) {
        srand(time(NULL));
        initialized = true;
    }
}

value_t bad_arg_type_error(value_t v, value_kind expected_k, obj_kind expected_o, const vm_ctx_t* ctx)
{
    return value_init_err(error_fmt(VM_ERR_WRONG_TYPE, ctx->line, ctx->alloc,
                                    "Wrong type of argument: expected %s, got %s",
                                    value_kind_str(expected_k, expected_o), value_type_name(v)),
                          ctx->alloc);
}

value_t ntv_print(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t v = values[0];
    sv_str_t str = !IS_STR(v) ? value_to_str(v, &vm->ctx) : AS_STR(v);
    printf("%.*s", (int)str.size, str.chars);

    if (!IS_STR(v))
        sv_str_deinit(&str, &sv_gpa);

    return value_nil;
}

value_t ntv_println(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t ret = ntv_print(values, n, vm);
    printf("\n");
    return ret;
}

value_t ntv_printf(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    if (!IS_STR(values[0]))
        return bad_arg_type_error(values[0], VALUE_OBJ, OBJ_STR, &vm->ctx);

    if (!IS_TUPLE(values[1]) && !IS_LIST(values[1]))
        return bad_arg_type_error(values[1], VALUE_OBJ, OBJ_TUPLE, &vm->ctx);

    iterable_t iter = iter_init(values[1], vm->ctx.alloc);
    if (iter.self == NULL)
        return value_init_err(error_fmt(VM_ERR_OOM, vm->ctx.line, vm->ctx.alloc, "OOM"), vm->ctx.alloc);
    sv_str_t fmt_string = AS_STR(values[0]);

    value_t ret = value_nil;
    bool failed = false;
    int64_t start_i = 0;
    int64_t i = 0;
    while (i < fmt_string.size) {
        char c = fmt_string.chars[i++];
        if (c != '%')
            continue;

        // fmt error
        if (i == fmt_string.size) {
            const char* err_msg = "Percent simbol '%%' without value descriptor. Use a double '%%%%' if you want to print '%%'";
            ret = value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc, err_msg),
                vm->ctx.alloc);
            failed = true;
            break;
        }

        c = fmt_string.chars[i++];
        if (c != '%' && c != 'v') {
            const char* err_msg = "Unknown value descriptor '%c'";
            ret = value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc, err_msg, c),
                vm->ctx.alloc);
            failed = true;
            break;
        }

        printf("%.*s", (int)(i - start_i - 2), fmt_string.chars + start_i);
        if (c == 'v') {
            sv_opt_t(value_t) print_value = iterable_next(&iter, vm->ctx.alloc);
            if (!print_value.is_some) {
                const char* err_msg = "No value left to print";
                ret = value_init_err(
                    error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc, err_msg),
                    vm->ctx.alloc);
                failed = true;
                break;
            }
            sv_str_t s = value_to_str(print_value.value, &vm->ctx);
            value_free(&print_value.value, vm->ctx.alloc);
            if (s.chars == NULL) {
                ret = value_init_err(
                    error_fmt(VM_ERR_OOM, vm->ctx.line, vm->ctx.alloc, "OOM"),
                    vm->ctx.alloc);
                failed = true;
                break;
            }
            printf("%.*s", (int)s.size, s.chars);
            sv_str_deinit(&s, vm->ctx.alloc);
        }
        else {
            printf("%%");
        }

        start_i = i;
    }
    if (!failed && start_i != i) {
        printf("%.*s", (int)(i - start_i), fmt_string.chars + start_i);
    }
    iter_deinit(&iter, vm->ctx.alloc);
    return ret;
}

value_t ntv_printlnf(const value_t* values, uint8_t n, vm_t* vm)
{
    value_t ret = ntv_printf(values, n, vm);
    if (!IS_ERR(ret))
        printf("\n");
    return ret;
}

value_t ntv_len(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t v = args[0];

    if (!IS_LIST(v) && !IS_STR(v) && !IS_MAP(v) && !IS_ITER(v))
        return bad_arg_type_error(v, VALUE_OBJ, OBJ_ITER, &vm->ctx);

    value_t ret = { .kind = VALUE_NUMBER, .number = 0 };

    switch (v.obj.cell->value.kind) {
    case OBJ_STR:
        ret.number = str_count(AS_STR(v));
        break;
    case OBJ_LIST:
        ret.number = ll_count(AS_LIST(v));
        break;
    case OBJ_MAP:
        ret.number = map_count(AS_MAP(v));
        break;
    case OBJ_ITER:
        ret.number = (double)iterable_len(&AS_ITER(v));
        break;
    case OBJ_TUPLE:
    case OBJ_ERR:
    case OBJ_RECORD:
    case OBJ_CLOSURE:
    case OBJ_CLOSURE_MEMBER:
    case OBJ_NATIVE_FN:
    case OBJ_USER_VALUE:
        return value_nil;
    }

    return ret;
}


#define RAND(formula) do {                                                     \
    assert(n == 2);                                                            \
    init_rand();                                                               \
    value_t min = args[0];                                                     \
    value_t max = args[1];                                                     \
    if (!IS_NUMBER(min))                                                       \
        return bad_arg_type_error(min, VALUE_NUMBER, OBJ_LIST, &vm->ctx);      \
    if (!IS_NUMBER(max))                                                       \
        return bad_arg_type_error(max, VALUE_NUMBER, OBJ_LIST, &vm->ctx);      \
    if (min.number > max.number)                                               \
        return value_nil;                                                      \
    double range = fabs(max.number - min.number);                              \
    double res = (formula);                                                    \
    return (value_t){ .number = res, .kind = VALUE_NUMBER };                   \
} while (0)

value_t ntv_randi(const value_t* args, uint8_t n, vm_t* vm)
{ RAND(rand() % (int)range + (int)min.number); }

value_t ntv_randf(const value_t* args, uint8_t n, vm_t* vm)
{ RAND((double)rand()/(double)(RAND_MAX/range) + min.number); }

value_t ntv_int(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t x = args[0];
    if (IS_NUMBER(x))
        return (value_t){ .kind = VALUE_NUMBER, .number = floor(AS_NUMBER(x)) };
    if (IS_ATOM(x))
        return (value_t){ .kind = VALUE_NUMBER, .number = x.number };
    return bad_arg_type_error(x, VALUE_NUMBER, OBJ_LIST, &vm->ctx);
}

#undef RAND
