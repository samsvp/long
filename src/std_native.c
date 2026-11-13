#include "std_native.h"

#include "obj/iterator.h"
#include "obj/iterable.h"
#include "value.h"
#include "std/allocator_std.h"
#include "obj/list.h"
#include "obj/str.h"
#include "vm.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdio.h>
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

value_t bad_arg_type_error(value_t v, value_kind expected_k, obj_kind expected_o, const vm_t* vm)
{
    return value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm_file(vm), vm_line(vm), vm->ctx.alloc,
                                    "Wrong type of argument: expected %s, got %s",
                                    value_kind_str(expected_k, expected_o), value_type_name(v)),
                          vm->ctx.alloc);
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

/**
 * (format fmt args): writes fmt to b, with each %v replaced by the next value of args and
 * %% by %. Returns nil, or the error.
 */
static value_t format(const value_t* args, sv_str_builder* b, vm_t* vm)
{
    if (!IS_STR(args[0]))
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_STR, vm);
    if (!IS_TUPLE(args[1]) && !IS_LIST(args[1]))
        return bad_arg_type_error(args[1], VALUE_OBJ, OBJ_TUPLE, vm);

    const sv_allocator_t* a = vm->ctx.alloc;
    iter_storage_t storage;
    iterable_t iter = iter_init_no_borrow(args[1], &storage);

    sv_str_t fmt = AS_STR(args[0]);
    value_t ret = value_nil;
    int64_t start_i = 0;
    int64_t i = 0;
    while (i < fmt.size) {
        if (fmt.chars[i++] != '%')
            continue;
        // the text before the %
        if (sv_strb_add(b, fmt.chars + start_i, i - 1 - start_i, a) < 0)
            goto oom;

        if (i == fmt.size) {
            const char* err_msg = "Percent simbol '%%' without value descriptor. Use a double '%%%%' if you want to print '%%'";
            ret = value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm_file(vm), vm_line(vm), a, err_msg), a);
            goto done;
        }

        char c = fmt.chars[i++];
        start_i = i;
        if (c == '%') {
            if (sv_strb_add_char(b, '%', a) < 0)
                goto oom;
            continue;
        }
        if (c != 'v') {
            ret = value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm_file(vm), vm_line(vm), a,
                                           "Unknown value descriptor '%c'", c), a);
            goto done;
        }

        sv_opt_t(value_t) v = iterable_next(&iter, a);
        if (!v.is_some) {
            ret = value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm_file(vm), vm_line(vm), a,
                                           "No value left to print"), a);
            goto done;
        }
        bool ok = value_write(v.value, b, &vm->ctx);
        value_free(&v.value, a);
        if (!ok)
            goto oom;
    }
    if (sv_strb_add(b, fmt.chars + start_i, fmt.size - start_i, a) >= 0)
        goto done;

oom:
    ret = VALUE_OOM;
done:
    return ret;
}

value_t ntv_printf(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    sv_str_builder b = sv_strb_init();
    value_t ret = format(values, &b, vm);
    if (!IS_ERR(ret) && b.size > 0)
        printf("%.*s", (int)b.size, b.arr);
    sv_strb_deinit(&b, vm->ctx.alloc);
    return ret;
}

value_t ntv_sprintf(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    sv_str_builder b = sv_strb_init();
    value_t ret = format(values, &b, vm);
    if (IS_ERR(ret)) {
        sv_strb_deinit(&b, vm->ctx.alloc);
        return ret;
    }
    ret = value_init_str_own(sv_strb_to_str(&b), vm->ctx.alloc);
    if (AS_OBJ(ret).cell == NULL)
        return VALUE_OOM;
    return ret;
}

value_t ntv_printlnf(const value_t* values, uint8_t n, vm_t* vm)
{
    value_t ret = ntv_printf(values, n, vm);
    if (!IS_ERR(ret))
        printf("\n");
    return ret;
}

value_t ntv_input(const value_t* values, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    if (!IS_STR(values[0]))
        return bad_arg_type_error(values[0], VALUE_OBJ, OBJ_STR, vm);

    printf("%.*s\n", (int)AS_STR(values[0]).size, AS_STR(values[0]).chars);

    sv_str_builder b = sv_strb_init();
    value_t ret = value_nil;
    char c;
    while (c = getchar(), c != '\n' && c != EOF) {
        if (sv_strb_add_char(&b, c, vm->ctx.alloc) == -1) {
            ret = VALUE_OOM;
            goto ret;
        }
    }

    sv_str_t str = sv_strb_to_str(&b);
    ret = value_init_str(str, vm->ctx.alloc);
    if (AS_OBJ(ret).cell == NULL)
        ret = VALUE_OOM;

ret:
    sv_strb_deinit(&b, vm->ctx.alloc);
    return ret;
}

value_t ntv_len(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t v = args[0];

    if (!IS_LIST(v) && !IS_STR(v) && !IS_MAP(v) && !IS_ITER(v))
        return bad_arg_type_error(v, VALUE_OBJ, OBJ_ITER, vm);

    double count = 0;

    switch (AS_OBJ(v).cell->value.kind) {
    case OBJ_STR:
        count = str_count(AS_STR(v));
        break;
    case OBJ_LIST:
        count = ll_count(AS_LIST(v));
        break;
    case OBJ_MAP:
        count = map_count(AS_MAP(v));
        break;
    case OBJ_ITER:
        count = (double)iterable_len(&AS_ITER(v));
        break;
    case OBJ_ACTOR:
    case OBJ_TUPLE:
    case OBJ_ERR:
    case OBJ_RECORD:
    case OBJ_CLOSURE:
    case OBJ_CLOSURE_MEMBER:
    case OBJ_NATIVE_FN:
    case OBJ_USER_VALUE:
        return value_nil;
    }

    return value_number(count);
}


#define RAND(formula) do {                                                     \
    assert(n == 2);                                                            \
    init_rand();                                                               \
    value_t min = args[0];                                                     \
    value_t max = args[1];                                                     \
    if (!IS_NUMBER(min))                                                       \
        return bad_arg_type_error(min, VALUE_NUMBER, OBJ_LIST, vm);            \
    if (!IS_NUMBER(max))                                                       \
        return bad_arg_type_error(max, VALUE_NUMBER, OBJ_LIST, vm);            \
    if (AS_NUMBER(min) > AS_NUMBER(max))                                       \
        return value_nil;                                                      \
    double range = fabs(AS_NUMBER(max) - AS_NUMBER(min));                      \
    double res = (formula);                                                    \
    return value_number(res);                                                  \
} while (0)

value_t ntv_randi(const value_t* args, uint8_t n, vm_t* vm)
{
    if (n == 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])
        && !(fabs(AS_NUMBER(args[1]) - AS_NUMBER(args[0])) < INT_MAX)
    )
        return value_nil;
    RAND(rand() % ((int)range + 1) + (int)AS_NUMBER(min));
}

value_t ntv_randf(const value_t* args, uint8_t n, vm_t* vm)
{ RAND((double)rand()/(double)(RAND_MAX/range) + AS_NUMBER(min)); }

value_t ntv_int(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    value_t x = args[0];
    if (IS_NUMBER(x))
        return value_number(floor(AS_NUMBER(x)));
    if (IS_ATOM(x))
        return value_number(AS_ATOM(x));
    if (IS_STR(x)) {
        sv_str_t s = AS_STR(x);
        const char* c = sv_str_to_c_str(s, vm->ctx.alloc);
        char* end = "";
        long int d = strtol(c, &end, 10);
        if (c == end || end[0] != '\0') {
            return value_nil;
        }
        return value_number((double)d);
    }
    return value_nil;
}

#undef RAND
