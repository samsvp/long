#include "list.h"
#include "../value.h"
#include "../std_native.h"
#include "../vm.h"
#include "../obj/list.h"
#include "../obj/iterator.h"
#include "../obj/iterable.h"
#include "math.h"
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static value_t map_filter_list(value_t value, value_t fn, bool with_index, bool filter, vm_t* vm)
{

    iterable_t iter = iter_init(value, vm->ctx.alloc);
    if (iter.self == NULL)
        return bad_arg_type_error(value, VALUE_OBJ, OBJ_ITER, &vm->ctx);

    int64_t count = iterable_len(&iter);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, count, vm->ctx.alloc);
    CHECK_OOM(vec.arr);

    sv_opt_t(value_t) v = sv_opt_none_t(value_t);
    double i = 0;
    while (v = iterable_next(&iter, vm->ctx.alloc), v.is_some) {
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
    iter_deinit(&iter, vm->ctx.alloc);

    if (vec.size < count)
        memmove(vec.arr, vec.arr + count - vec.size, vec.size * sizeof(value_t));
    return value_from_list(vec, vm);
}

static value_t reduce_list(value_t value, value_t fn, value_t acc, bool with_index, vm_t* vm)
{
    iterable_t iter = iter_init(value, vm->ctx.alloc);
    if (iter.self == NULL)
        return bad_arg_type_error(value, VALUE_OBJ, OBJ_ITER, &vm->ctx);

    sv_opt_t(value_t) v = sv_opt_none_t(value_t);
    double i = 0;
    while (v = iterable_next(&iter, vm->ctx.alloc), v.is_some) {
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
    return map_filter_list(args[0], args[1], false, false, vm);
}

value_t ntvl_filter(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    return map_filter_list(args[0], args[1], false, true, vm);
}

value_t ntvl_reduce(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return reduce_list(args[0], args[1], args[2], false, vm);
}

value_t ntvl_imap(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    return map_filter_list(args[0], args[1], true, false, vm);
}

value_t ntvl_ifilter(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    return map_filter_list(args[0], args[1], true, true, vm);
}

value_t ntvl_ireduce(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return reduce_list(args[0], args[1], args[2], true, vm);
}

value_t ntvl_zip(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    const sv_allocator_t* a = vm->ctx.alloc;

    iterable_t iter1 = iter_init(args[0], a);
    iterable_t iter2 = iter_init(args[1], a);
    if (iter1.self == NULL)
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ITER, &vm->ctx);
    if (iter2.self == NULL)
        return bad_arg_type_error(args[1], VALUE_OBJ, OBJ_ITER, &vm->ctx);

    int64_t l1_n = iterable_len(&iter1);
    int64_t l2_n = iterable_len(&iter2);

    if (l1_n != l2_n)
        return value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, a,
                          "Lists must have the same length, got %ld and %ld",
                          l1_n, l2_n),
                a);

    sv_opt_t(value_t) v1 = sv_opt_none_t(value_t);
    sv_opt_t(value_t) v2 = sv_opt_none_t(value_t);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, l1_n, a);
    CHECK_OOM(vec.arr);
    while (v1 = iterable_next(&iter1, a), v2 = iterable_next(&iter2, a), v1.is_some) {
        value_t z[] = { v1.value, v2.value };
        value_t tuple = value_init_tuple(z, 2, a);
        CHECK_OOM(tuple.obj.cell);
        vec.arr[l1_n - ++vec.size] = tuple;
    }

    return value_from_list(vec, vm);
}

static value_t map_window(value_t val, value_t f, value_t window_len,
                          bool should_filter, vm_t* vm)
{
    CHECK_LIST(val);
    CHECK_NUMBER(window_len);

    double w = AS_NUMBER(window_len);
    if (w < 1 || w != floor(w))
        return value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc,
                          "Window size must be a positive integer, got %g", w),
                vm->ctx.alloc);
    int32_t w_size = w;

    list_t l = AS_LIST(val);

    int64_t count = ll_count(l);
    if (w > count) {
        value_t ret = value_wrap_list(ll_empty(), vm->ctx.alloc);
        CHECK_OOM(ret.obj.cell);
        return ret;
    }

    int64_t size = count - w_size + 1;
    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, size, vm->ctx.alloc);
    CHECK_OOM(vec.arr);

    int64_t current_index = 0;
    for (int64_t i = 0; i < size; i++) {
        list_t slice = ll_slice(l, i, i + w_size, vm->ctx.alloc);
        CHECK_OOM(slice.cell);

        value_t vslice = value_wrap_list(slice, vm->ctx.alloc);
        CHECK_OOM(vslice.obj.cell);

        value_t ret = vm_call_value(vm, f, &vslice, 1);
        value_free(&vslice, vm->ctx.alloc);

        if (should_filter && IS_FALSEY(ret))
            continue;

        UNWRAP_ERR(ret);
        vec.arr[size - ++current_index] = ret;
    }
    vec.size = current_index;
    if (vec.size < size)
        memmove(vec.arr, vec.arr + size - vec.size, vec.size * sizeof(value_t));

    return value_from_list(vec, vm);

}

value_t ntvl_map_window(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return map_window(args[0], args[1], args[2], false, vm);
}

value_t ntvl_mapf_window(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return map_window(args[0], args[1], args[2], true, vm);
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

    list_t l = AS_LIST(args[0]);
    list_t list = ll_slice(l, AS_NUMBER(args[1]), ll_count(l), vm->ctx.alloc);
    CHECK_OOM(list.cell);
    value_t ret = value_wrap_list(list, vm->ctx.alloc);
    CHECK_OOM(ret.obj.cell);
    return ret;
}

static int value_num_cmp(const void* a, const void* b)
{
    double x = ((const value_t*)a)->number;
    double y = ((const value_t*)b)->number;
    return (x < y) - (x > y);
}

static value_t sort(value_t value, const value_t* f, vm_t* vm)
{
    CHECK_LIST(value);
    list_t l = AS_LIST(value);
    int64_t count = ll_count(l);
    if (count <= 1)
        return value_borrow(value);

    sv_vec_t(value_t) vs = sv_vec_init_capacity(value_t, count, vm->ctx.alloc);

    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) maybe_x = sv_opt_none_t(value_t);
    while (maybe_x = ll_iter_next(&iter), maybe_x.is_some) {
        value_t x = maybe_x.value;
        value_t ret = f == NULL ? x : vm_call_value(vm, *f, &x, 1);
        UNWRAP_ERR(ret);
        if (!IS_NUMBER(ret)) {
            sv_vec_deinit(&vs, vm->ctx.alloc);
            value_free(&ret, vm->ctx.alloc);
            return bad_arg_type_error(x, VALUE_NUMBER, OBJ_LIST, &vm->ctx);                                          \
        }
        vs.arr[vs.size++] = x;
    }

    qsort(vs.arr, vs.size, sizeof(value_t), value_num_cmp);
    return value_from_list(vs, vm);
}

value_t ntvl_sort(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);
    return sort(args[0], NULL, vm);
}

value_t ntvl_sort_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    return sort(args[0], &args[1], vm);
}

value_t ntvl_all(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    list_t l = AS_LIST(args[0]);
    int64_t count = ll_count(l);
    if (count == 0)
        return value_true;

    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) maybe_x = sv_opt_none_t(value_t);
    while (maybe_x = ll_iter_next(&iter), maybe_x.is_some) {
        value_t x = maybe_x.value;
        value_t ret = vm_call_value(vm, args[1], &x, 1);
        UNWRAP_ERR(ret);
        if (IS_FALSEY(ret))
            return value_false;
    }

    return value_true;
}
