#include "list.h"
#include "../value.h"
#include "../std_native.h"
#include "../vm.h"
#include "../obj/list.h"
#include "../obj/iterator.h"
#include "../obj/iterable.h"
#include "math.h"
#include "../stable_sort.h"
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

static value_t wrap_list(list_t list, vm_t* vm)
{
    CHECK_OOM(list.cell);
    value_t ret = value_wrap_list(list, vm->ctx.alloc);
    if (ret.obj.cell == NULL) {
        ll_deinit(&list, vm->ctx.alloc);
        return oom_error(&vm->ctx);
    }
    return ret;
}

static value_t value_from_list(sv_vec_t(value_t) vec, vm_t* vm)
{
    return wrap_list(ll_init_from_vec_rev(vec, vm->ctx.alloc), vm);
}

/**
 * Frees a vector filled from its back
 */
static void free_back_filled(sv_vec_t(value_t)* vec, const sv_allocator_t* a)
{
    for (int64_t i = vec->capacity - vec->size; i < vec->capacity; i++)
        value_free(&vec->arr[i], a);
    sv_vec_deinit(vec, a);
}

static value_t map_filter_list(value_t value, value_t fn, bool with_index, bool filter, vm_t* vm)
{
    const sv_allocator_t* a = vm->ctx.alloc;

    iterable_t iter = iter_init(value, a);
    if (iter.self == NULL)
        return bad_arg_type_error(value, VALUE_OBJ, OBJ_ITER, &vm->ctx);

    int64_t count = iterable_len(&iter);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, count, a);
    if (vec.arr == NULL) {
        iter_deinit(&iter, a);
        return oom_error(&vm->ctx);
    }

    sv_opt_t(value_t) v = sv_opt_none_t(value_t);
    double i = 0;
    while (v = iterable_next(&iter, a), v.is_some) {
        value_t args[] = { v.value, (value_t){ .kind = VALUE_NUMBER, .number = i++ } };
        value_t ret = vm_call_value(vm, fn, args, with_index ? 2 : 1);
        if (IS_ERR(ret)) {
            value_free(&v.value, a);
            free_back_filled(&vec, a);
            iter_deinit(&iter, a);
            return ret;
        }
        if (!filter) {
            vec.arr[count - ++vec.size] = ret;
            value_free(&v.value, a);
        } else {
            if (!IS_FALSEY(ret))
                vec.arr[count - ++vec.size] = v.value;
            else
                value_free(&v.value, a);
            value_free(&ret, a);
        }
    }
    iter_deinit(&iter, a);

    if (vec.size < count)
        memmove(vec.arr, vec.arr + count - vec.size, vec.size * sizeof(value_t));
    return value_from_list(vec, vm);
}

static value_t reduce_list(value_t value, value_t fn, value_t acc, bool with_index, vm_t* vm)
{
    const sv_allocator_t* a = vm->ctx.alloc;

    iterable_t iter = iter_init(value, a);
    if (iter.self == NULL)
        return bad_arg_type_error(value, VALUE_OBJ, OBJ_ITER, &vm->ctx);

    acc = value_borrow(acc);
    sv_opt_t(value_t) v = sv_opt_none_t(value_t);
    double i = 0;
    while (v = iterable_next(&iter, a), v.is_some) {
        value_t args[] = { v.value, acc, (value_t){ .kind = VALUE_NUMBER, .number = i++ } };
        value_t new_acc = vm_call_value(vm, fn, args, with_index ? 3 : 2);
        value_free(&v.value, a);
        value_free(&acc, a);
        acc = new_acc;
        if (IS_ERR(acc))
            break;
    }
    iter_deinit(&iter, a);

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

value_t ntvl_add(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    CHECK_LIST(args[0]);
    CHECK_LIST(args[1]);
    list_t list = ll_add(AS_LIST(args[0]), AS_LIST(args[1]), vm->ctx.alloc);
    CHECK_OOM(list.cell);
    value_t value = value_wrap_list(list, vm->ctx.alloc);
    CHECK_OOM(value.obj.cell);
    return value;
}

value_t ntvl_zip(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    const sv_allocator_t* a = vm->ctx.alloc;

    iterable_t iter1 = iter_init(args[0], a);
    if (iter1.self == NULL)
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ITER, &vm->ctx);
    iterable_t iter2 = iter_init(args[1], a);
    if (iter2.self == NULL) {
        iter_deinit(&iter1, a);
        return bad_arg_type_error(args[1], VALUE_OBJ, OBJ_ITER, &vm->ctx);
    }

    int64_t l1_n = iterable_len(&iter1);
    int64_t l2_n = iterable_len(&iter2);

    if (l1_n != l2_n) {
        iter_deinit(&iter1, a);
        iter_deinit(&iter2, a);
        return value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, a,
                          "Lists must have the same length, got %ld and %ld",
                          l1_n, l2_n),
                a);
    }

    sv_opt_t(value_t) v1 = sv_opt_none_t(value_t);
    sv_opt_t(value_t) v2 = sv_opt_none_t(value_t);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, l1_n, a);
    bool oom = vec.arr == NULL;
    while (!oom && (v1 = iterable_next(&iter1, a), v2 = iterable_next(&iter2, a), v1.is_some)) {
        value_t z[] = { v1.value, v2.value };
        value_t tuple = value_init_tuple(z, 2, a);
        value_free(&v1.value, a);
        value_free(&v2.value, a);
        oom = tuple.obj.cell == NULL;
        if (!oom)
            vec.arr[l1_n - ++vec.size] = tuple;
    }
    iter_deinit(&iter1, a);
    iter_deinit(&iter2, a);

    if (oom) {
        free_back_filled(&vec, a);
        return oom_error(&vm->ctx);
    }
    return value_from_list(vec, vm);
}

static value_t map_window(value_t val, value_t f, value_t window_len,
                          bool should_map, bool should_filter, vm_t* vm)
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

    for (int64_t i = 0; i < size; i++) {
        value_t vslice = wrap_list(ll_slice(l, i, i + w_size, vm->ctx.alloc), vm);
        if (IS_ERR(vslice)) {
            free_back_filled(&vec, vm->ctx.alloc);
            return vslice;
        }

        value_t ret = vm_call_value(vm, f, &vslice, 1);
        if (IS_ERR(ret)) {
            value_free(&vslice, vm->ctx.alloc);
            free_back_filled(&vec, vm->ctx.alloc);
            return ret;
        }

        if (should_filter && IS_FALSEY(ret)) {
            value_free(&vslice, vm->ctx.alloc);
            continue;
        }

        if (should_map) {
            value_free(&vslice, vm->ctx.alloc);
            vec.arr[size - ++vec.size] = ret;
        } else {
            value_free(&ret, vm->ctx.alloc);
            vec.arr[size - ++vec.size] = vslice;
        }
    }
    if (vec.size < size)
        memmove(vec.arr, vec.arr + size - vec.size, vec.size * sizeof(value_t));

    return value_from_list(vec, vm);

}

value_t ntvl_map_window(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return map_window(args[0], args[1], args[2], true, false, vm);
}

value_t ntvl_mapf_window(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return map_window(args[0], args[1], args[2], true, true, vm);
}

value_t ntvl_filter_window(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 3);
    return map_window(args[0], args[1], args[2], false, true, vm);
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

    return wrap_list(ll_slice(AS_LIST(args[0]), 0, AS_NUMBER(args[1]), vm->ctx.alloc), vm);
}

value_t ntvl_skip(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    CHECK_NUMBER(args[1]);

    list_t l = AS_LIST(args[0]);
    return wrap_list(ll_slice(l, AS_NUMBER(args[1]), ll_count(l), vm->ctx.alloc), vm);
}

value_t ntvl_combinations(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    CHECK_LIST(args[0]);
    CHECK_NUMBER(args[1]);

    const sv_allocator_t* a = vm->ctx.alloc;

    double k = AS_NUMBER(args[1]);
    if (k < 0 || k != floor(k))
        return value_init_err(
                error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, a,
                          "Combination size must be a non-negative integer, got %g", k),
                a);

    list_t l = AS_LIST(args[0]);
    int64_t count = ll_count(l);
    if (k > count)
        return value_borrow(args[0]);
    int64_t k_size = k;

    if (k_size == 0) {
        value_t empty = value_wrap_list(ll_empty(), a);
        CHECK_OOM(empty.obj.cell);
        sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, 1, a);
        if (vec.arr == NULL) {
            value_free(&empty, a);
            return oom_error(&vm->ctx);
        }
        vec.arr[vec.size++] = empty;
        return value_from_list(vec, vm);
    }

    int64_t total = 1;
    for (int64_t i = 0; i < k_size; i++) {
        if (total > (int64_t)(SIZE_MAX / sizeof(value_t) / count))
            return oom_error(&vm->ctx);
        total = total * (count - i) / (i + 1);
    }

    sv_vec_t(value_t) elems = sv_vec_init_capacity(value_t, count, a);
    sv_vec_t(int64_t) idx = sv_vec_init_capacity(int64_t, k_size, a);
    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, total, a);
    if (elems.arr == NULL || idx.arr == NULL || vec.arr == NULL)
        goto oom;

    ll_iter_t iter = ll_iter_init_no_borrow(l);
    sv_opt_t(value_t) maybe_x = sv_opt_none_t(value_t);
    while (maybe_x = ll_iter_next(&iter), maybe_x.is_some)
        elems.arr[elems.size++] = maybe_x.value;

    for (int64_t i = 0; i < k_size; i++)
        idx.arr[idx.size++] = i;

    for (;;) {
        sv_vec_t(value_t) comb = sv_vec_init_capacity(value_t, k_size, a);
        if (comb.arr == NULL)
            goto oom;
        for (int64_t i = 0; i < k_size; i++)
            comb.arr[comb.size++] = value_borrow(elems.arr[idx.arr[k_size - i - 1]]);
        value_t c = value_from_list(comb, vm);
        if (IS_ERR(c)) {
            value_free(&c, a);
            goto oom;
        }
        vec.arr[total - ++vec.size] = c;

        int64_t j = k_size - 1;
        while (j >= 0 && idx.arr[j] == count - k_size + j)
            j--;
        if (j < 0)
            break;
        idx.arr[j]++;
        for (int64_t t = j + 1; t < k_size; t++)
            idx.arr[t] = idx.arr[t - 1] + 1;
    }

    sv_vec_deinit(&elems, a);
    sv_vec_deinit(&idx, a);
    return value_from_list(vec, vm);

oom:
    free_back_filled(&vec, a);
    sv_vec_deinit(&elems, a);
    sv_vec_deinit(&idx, a);
    return oom_error(&vm->ctx);
}

static int value_num_cmp(void* ctx, const void* a, const void* b)
{
    (void)ctx;
    double x = ((const value_t*)a)->number;
    double y = ((const value_t*)b)->number;
    return (x < y) - (x > y);
}

typedef struct {
    vm_t* vm;
    value_t f;
    sv_opt_t(error_t) err;
} sort_ctx_t;

static int keyed_cmp(void* mctx, const void* a, const void* b)
{
    sort_ctx_t* ctx = mctx;
    value_t ta = vm_call_value(ctx->vm, ctx->f, a, 1);
    value_t tb = vm_call_value(ctx->vm, ctx->f, b, 1);
    if (IS_ERR(ta))
        ctx->err = sv_opt_some_t(error_t, AS_ERR(ta));
    if (IS_ERR(tb))
        ctx->err = sv_opt_some_t(error_t, AS_ERR(tb));
    if (ctx->err.is_some)
        return 0;

    if (!IS_NUMBER(ta) || !IS_NUMBER(tb)) {
        value_t x = IS_NUMBER(ta) ? tb : ta;
        error_t err = AS_ERR(bad_arg_type_error(x, VALUE_NUMBER, OBJ_LIST, &ctx->vm->ctx));
        ctx->err = sv_opt_some_t(error_t, err);
    }

    double x = AS_NUMBER(ta);
    double y = AS_NUMBER(tb);
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
        value_t x = value_borrow(maybe_x.value);
        vs.arr[count - ++vs.size] = x;
    }

    sort_ctx_t ctx = {
        .vm = vm,
        .f = f != NULL ? *f : value_nil,
        .err = sv_opt_none,
    };
    stable_sort(vs.arr, vs.size, sizeof(value_t), &ctx, f == NULL ? value_num_cmp : keyed_cmp);
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

static value_t any_all(const value_t* args, uint8_t n, bool is_all, vm_t* vm)
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
        if (is_all) {
            if (IS_FALSEY(ret)) {
                value_free(&ret, vm->ctx.alloc);
                return value_false;
            }
        } else if (!IS_FALSEY(ret)) {
            value_free(&ret, vm->ctx.alloc);
            return value_true;
        }
        value_free(&ret, vm->ctx.alloc);
    }

    return is_all ? value_true : value_false;
}

value_t ntvl_all(const value_t* args, uint8_t n, vm_t* vm)
{ return any_all(args, n, true, vm); }

value_t ntvl_any(const value_t* args, uint8_t n, vm_t* vm)
{ return any_all(args, n, false, vm); }

static value_t min_max(const value_t* args, uint8_t n, bool is_max, vm_t* vm)
{
    assert(n == 1 || n == 2);

    CHECK_LIST(args[0]);
    const value_t* f = n == 2 ? &args[1] : NULL;

    ll_iter_t iter = ll_iter_init_no_borrow(AS_LIST(args[0]));
    sv_opt_t(value_t) maybe_x = sv_opt_none_t(value_t);
    value_t best = value_nil;
    double best_key = -INFINITY;
    while (maybe_x = ll_iter_next(&iter), maybe_x.is_some) {
        value_t x = maybe_x.value;
        value_t key = f == NULL ? x : vm_call_value(vm, *f, &x, 1);
        UNWRAP_ERR(key);
        if (!IS_NUMBER(key)) {
            value_t err = bad_arg_type_error(key, VALUE_NUMBER, OBJ_LIST, &vm->ctx);
            if (f != NULL)
                value_free(&key, vm->ctx.alloc);
            return err;
        }
        if (is_max ? key.number > best_key : key.number < best_key) {
            best = x;
            best_key = key.number;
        }
    }

    return value_borrow(best);
}

value_t ntvl_min(const value_t* args, uint8_t n, vm_t* vm)
{ return min_max(args, n, false, vm); }

value_t ntvl_max(const value_t* args, uint8_t n, vm_t* vm)
{ return min_max(args, n, true, vm); }

value_t ntvl_min_by(const value_t* args, uint8_t n, vm_t* vm)
{ return min_max(args, n, false, vm); }

value_t ntvl_max_by(const value_t* args, uint8_t n, vm_t* vm)
{ return min_max(args, n, true, vm); }

value_t ntvl_count(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    iterable_t iter = iter_init(args[0], vm->ctx.alloc);
    if (iter.self == NULL)
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ITER, &vm->ctx);

    double count = 0;
    sv_opt_t(value_t) next = sv_opt_none_t(value_t);
    while (next = iterable_next(&iter, vm->ctx.alloc), next.is_some)
        count += value_eql(next.value, args[1]);

    return (value_t){.kind = VALUE_NUMBER, .number = count};
}

value_t ntvl_count_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);

    iterable_t iter = iter_init(args[0], vm->ctx.alloc);
    if (iter.self == NULL)
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ITER, &vm->ctx);

    double count = 0;
    sv_opt_t(value_t) next = sv_opt_none_t(value_t);
    while (next = iterable_next(&iter, vm->ctx.alloc), next.is_some) {
        value_t ret = vm_call_value(vm, args[1], &next.value, 1);
        UNWRAP_ERR(ret);
        count += !IS_FALSEY(ret);
        value_free(&ret, vm->ctx.alloc);
    }

    return (value_t){.kind = VALUE_NUMBER, .number = count};
}

value_t ntvl_rev(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);

    const sv_allocator_t* a = vm->ctx.alloc;

    iterable_t iter = iter_init(args[0], a);
    if (iter.self == NULL)
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ITER, &vm->ctx);

    int64_t count = iterable_len(&iter);

    sv_vec_t(value_t) vec = sv_vec_init_capacity(value_t, count, a);
    if (vec.arr == NULL) {
        iter_deinit(&iter, a);
        return oom_error(&vm->ctx);
    }

    sv_opt_t(value_t) v = sv_opt_none_t(value_t);
    while (v = iterable_next(&iter, a), v.is_some)
        vec.arr[vec.size++] = value_borrow(v.value);

    iter_deinit(&iter, a);
    return value_from_list(vec, vm);
}
