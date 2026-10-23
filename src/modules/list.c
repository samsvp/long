#include "list.h"
#include "../value.h"
#include "../std_native.h"
#include "../vm.h"
#include "../obj/list.h"
#include "math.h"
#include <assert.h>
#include <stdlib.h>

static value_t oom_error(const vm_ctx_t* ctx)
{
    return value_init_err(error_fmt(VM_ERR_OOM, ctx->line, ctx->alloc, "OOM"), ctx->alloc);
}

#define CHECK_OOM(v) do { if ((v) == NULL) return oom_error(&vm->ctx); } while (0)

#define UNWRAP_ERR(v) do {                                                                                    \
    if (IS_ERR(v)) return v;                                                                                  \
} while (0);

#define CHECK_LIST(v) do {                                                                                    \
    if (!IS_LIST(v))                                                                                          \
        return bad_arg_type_error(v, VALUE_OBJ, OBJ_LIST, &vm->ctx);                                          \
} while (0)

static value_t value_from_list(sv_vec_t(value_t) vec, vm_t* vm)
{
    list_t new_list = ll_init_from_vec_rev(vec, vm->ctx.alloc);
    CHECK_OOM(new_list.cell);
    value_t ret = value_wrap_list(new_list, vm->ctx.alloc);
    CHECK_OOM(ret.obj.cell);
    return ret;
}

static value_t map_filter_list(list_t l, value_t fn, bool with_index, bool filter, vm_t* vm)
{
    int64_t count = ll_count(l);
    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, count, vm->ctx.alloc);
    CHECK_OOM(vec.arr);

    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) v = sv_opt_none_t(value_t);

    double i = 0;
    while (v = ll_iter_next(&iter), v.is_some) {
        value_t args[] = { v.value, (value_t){ .kind = VALUE_NUMBER, .number = i++ } };
        value_t ret = vm_call_value(vm, fn, args, with_index ? 2 : 1);
        UNWRAP_ERR(ret);
        if (!filter)
            vec.arr[count - ++vec.size] = ret;
        else if (!IS_FALSEY(ret)) {
            vec.arr[count - ++vec.size] = value_borrow(v.value);
            value_free(&ret, vm->ctx.alloc);
        }
    }

    return value_from_list(vec, vm);
}

static value_t reduce_list(list_t l, value_t fn, value_t acc, bool with_index, vm_t* vm)
{
    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) v = sv_opt_none_t(value_t);

    double i = 0;
    while (v = ll_iter_next(&iter), v.is_some) {
        value_t args[] = { v.value, acc, (value_t){ .kind = VALUE_NUMBER, .number = i++ } };
        value_t new_acc = vm_call_value(vm, fn, args, with_index ? 3 : 2);
        UNWRAP_ERR(new_acc);
        value_free(&acc, vm->ctx.alloc);
        acc = new_acc;
    }

    return acc;
}

value_t ntvl_map(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);

    return map_filter_list(AS_LIST(args[0]), args[1], false, false, vm);
}

value_t ntvl_filter(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);

    return map_filter_list(AS_LIST(args[0]), args[1], false, true, vm);
}

value_t ntvl_reduce(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);

    CHECK_LIST(args[0]);

    return reduce_list(AS_LIST(args[0]), args[1], args[2], false, vm);
}

value_t ntvl_imap(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);

    return map_filter_list(AS_LIST(args[0]), args[1], true, false, vm);
}

value_t ntvl_ifilter(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);

    return map_filter_list(AS_LIST(args[0]), args[1], true, true, vm);
}

value_t ntvl_ireduce(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);

    CHECK_LIST(args[0]);

    return reduce_list(AS_LIST(args[0]), args[1], args[2], true, vm);
}

value_t ntvl_zip(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    CHECK_LIST(args[1]);

    list_t l1 = AS_LIST(args[0]);
    list_t l2 = AS_LIST(args[1]);

    int64_t l1_n = ll_count(l1);
    int64_t l2_n = ll_count(l2);

    if (l1_n != l2_n)
        return value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc,
                          "Lists must have the same length, got %ld and %ld",
                          l1_n, l2_n),
                vm->ctx.alloc);

    ll_iter_t iter_1 = ll_iter_init_no_borrow(l1);
    ll_iter_t iter_2 = ll_iter_init_no_borrow(l2);

    sv_opt_t(value_t) v1 = sv_opt_none_t(value_t);
    sv_opt_t(value_t) v2 = sv_opt_none_t(value_t);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, l1_n, vm->ctx.alloc);
    CHECK_OOM(vec.arr);
    while (v1 = ll_iter_next(&iter_1), v2 = ll_iter_next(&iter_2), v1.is_some) {
        value_t z[] = { v1.value, v2.value };
        value_t tuple = value_init_tuple(z, 2, vm->ctx.alloc);
        CHECK_OOM(tuple.obj.cell);
        vec.arr[l1_n - ++vec.size] = tuple;
    }

    return value_from_list(vec, vm);
}

value_t ntvl_shuffle(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    init_rand();

    CHECK_LIST(args[0]);

    list_t l = AS_LIST(args[0]);
    int64_t count = ll_count(l);
    if (count <= 1)
        return value_borrow(args[0]);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, count, vm->ctx.alloc);
    CHECK_OOM(vec.arr);

    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) maybe_x = sv_opt_none_t(value_t);
    int64_t i = 0;
    while (maybe_x = ll_iter_next(&iter), maybe_x.is_some) {
        int j = rand() % (i + 1);
        value_t x = value_borrow(maybe_x.value);
        if (j == i) {
            vec.arr[i] = x;
        } else {
            vec.arr[i] = vec.arr[j];
            vec.arr[j] = x;
        }
        i++;
    }
    vec.size = count;

    return value_from_list(vec, vm);
}

value_t ntvl_take(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    CHECK_NUMBER(args[1]);

    list_t list = ll_slice(AS_LIST(args[0]), 0, AS_NUMBER(args[1]), vm->ctx.alloc);
    CHECK_OOM(list.cell);
    value_t ret = value_wrap_list(list, vm->ctx.alloc);
    CHECK_OOM(ret.obj.cell);
    return ret;
}

value_t ntvl_skip(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    CHECK_NUMBER(args[1]);

    list_t list = ll_slice(AS_LIST(args[0]), AS_NUMBER(args[1]), -1, vm->ctx.alloc);
    CHECK_OOM(list.cell);
    value_t ret = value_wrap_list(list, vm->ctx.alloc);
    CHECK_OOM(ret.obj.cell);
    return ret;
}
