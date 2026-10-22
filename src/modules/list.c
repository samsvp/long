#include "list.h"
#include "../value.h"
#include "../std_native.h"
#include "../stable_sort.h"
#include "../obj/list.h"
#include "../obj/str.h"
#include "../vm.h"
#include <assert.h>
#include <string.h>

#define NUMBER(x) (value_t){ .kind = VALUE_NUMBER, .number = (x) }
// CALL_NATIVE reports a NULL cell as out of memory
#define OOM (value_t){ .kind = VALUE_OBJ }

#define CHECK_SEQ(v) do {                                                      \
    if (!IS_LIST(v) && !IS_STR(v))                                             \
        return not_seq_error(v, vm);                                           \
} while (0)

#define CHECK_NUMBER(v) do {                                                   \
    if (!IS_NUMBER(v))                                                         \
        return bad_arg_type_error(v, VALUE_NUMBER, OBJ_LIST, &vm->ctx);        \
} while (0)

typedef struct {
    value_t key;
    value_t value;
} keyed_t;

sv_vec_def(keyed_t);

/**
 * Iterates a list, or the chars of a string. Doesn't own the sequence.
 */
typedef struct {
    value_t seq;
    ll_iter_t list;
    int64_t offset; // byte offset into the string
    bool oom; // a char failed to allocate
} cursor_t;

/**
 * An error, or OOM.
 */
static bool failed(value_t v)
{
    return v.kind == VALUE_OBJ && (v.obj.cell == NULL || IS_ERR(v));
}

static value_t not_seq_error(value_t v, const vm_t* vm)
{
    return value_init_err(error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc,
                                    "Wrong type of argument: expected list or string, got %s",
                                    value_type_name(v)),
                          vm->ctx.alloc);
}

static int64_t seq_count(value_t seq)
{
    return IS_LIST(seq) ? ll_count(AS_LIST(seq)) : str_count(AS_STR(seq));
}

static cursor_t cursor(value_t seq)
{
    cursor_t c = { .seq = seq };
    if (IS_LIST(seq))
        c.list = ll_iter_init_no_borrow(AS_LIST(seq));
    return c;
}

/**
 * The next element, owned by the caller. False when done.
 */
static bool cursor_next(cursor_t* c, value_t* out, const sv_allocator_t* a)
{
    if (IS_LIST(c->seq)) {
        sv_opt_t(value_t) v = ll_iter_next(&c->list);
        if (v.is_some)
            *out = value_borrow(v.value);
        return v.is_some;
    }
    sv_str_t s = AS_STR(c->seq);
    if (c->offset >= s.size)
        return false;
    sv_str_t ch = str_get(sv_str_slice(s, c->offset, s.size), 0);
    c->offset += ch.size;
    *out = value_init_str(ch, a);
    c->oom = out->obj.cell == NULL;
    return !c->oom;
}

static bool values_init(value_arr* vs, int64_t n, const sv_allocator_t* a)
{
    *vs = (value_arr)sv_vec_init_capacity(value_t, n, a);
    return vs->arr != NULL || n == 0;
}

static value_t wrap_ll(list_t l, const sv_allocator_t* a)
{
    if (l.cell == NULL)
        return OOM;
    value_t v = value_wrap_list(l, a);
    if (v.obj.cell == NULL)
        ll_deinit(&l, a);
    return v;
}

static value_t wrap_list(value_arr vs, const sv_allocator_t* a)
{
    return wrap_ll(ll_init_from_vec(vs, a), a);
}

/**
 * A string if seq is one, otherwise a list. Takes the values.
 */
static value_t wrap_like(value_t seq, value_arr vs, const sv_allocator_t* a)
{
    if (!IS_STR(seq))
        return wrap_list(vs, a);
    sv_str_builder b = sv_strb_init();
    bool ok = true;
    for (int64_t i = 0; i < vs.size && ok; i++)
        ok = sv_strb_add(&b, AS_STR(vs.arr[i]).chars, AS_STR(vs.arr[i]).size, a) >= 0;
    value_arr_deinit(&vs, a);
    if (!ok) {
        sv_strb_deinit(&b, a);
        return OOM;
    }
    return value_init_str_own(sv_strb_to_str(&b), a);
}

/**
 * Both must be strings. An empty side returns the other, which also keeps sv_str_add from
 * allocating zero bytes.
 */
static value_t str_concat(value_t left, value_t right, const sv_allocator_t* a)
{
    if (AS_STR(right).size == 0)
        return value_borrow(left);
    if (AS_STR(left).size == 0)
        return value_borrow(right);
    sv_str_t s = sv_str_add(AS_STR(left), AS_STR(right), a);
    return s.size < 0 ? OOM : value_init_str_own(s, a);
}

/**
 * f(x), or f(x, i) when indexed.
 */
static value_t call_at(vm_t* vm, value_t f, value_t x, int64_t i, bool indexed)
{
    value_t call_args[] = { x, NUMBER(i) };
    return vm_call_value(vm, f, call_args, indexed ? 2 : 1);
}

static value_t map(const value_t* args, vm_t* vm, bool indexed)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t f = args[1];
    CHECK_SEQ(seq);
    value_arr ys;
    if (!values_init(&ys, seq_count(seq), a))
        return OOM;

    cursor_t it = cursor(seq);
    value_t x;
    for (int64_t i = 0; cursor_next(&it, &x, a); i++) {
        value_t y = call_at(vm, f, x, i, indexed);
        value_free(&x, a);
        if (failed(y)) {
            value_arr_deinit(&ys, a);
            return y;
        }
        ys.arr[ys.size++] = y;
    }
    if (it.oom) {
        value_arr_deinit(&ys, a);
        return OOM;
    }
    return wrap_list(ys, a);
}

static value_t filter(const value_t* args, vm_t* vm, bool indexed)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t f = args[1];
    CHECK_SEQ(seq);
    value_arr kept;
    if (!values_init(&kept, seq_count(seq), a))
        return OOM;

    cursor_t it = cursor(seq);
    value_t x;
    for (int64_t i = 0; cursor_next(&it, &x, a); i++) {
        value_t keep = call_at(vm, f, x, i, indexed);
        if (failed(keep)) {
            value_free(&x, a);
            value_arr_deinit(&kept, a);
            return keep;
        }
        if (value_is_truthy(keep))
            kept.arr[kept.size++] = x;
        else
            value_free(&x, a);
        value_free(&keep, a);
    }
    if (it.oom) {
        value_arr_deinit(&kept, a);
        return OOM;
    }
    return wrap_like(seq, kept, a);
}

static value_t reduce(const value_t* args, vm_t* vm, bool indexed)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t f = args[1];
    CHECK_SEQ(seq);

    value_t acc = value_borrow(args[2]);
    cursor_t it = cursor(seq);
    value_t x;
    for (int64_t i = 0; !failed(acc) && cursor_next(&it, &x, a); i++) {
        value_t call_args[] = { x, acc, NUMBER(i) };
        value_t next = vm_call_value(vm, f, call_args, indexed ? 3 : 2);
        value_free(&x, a);
        value_free(&acc, a);
        acc = next;
    }
    if (it.oom) {
        value_free(&acc, a);
        return OOM;
    }
    return acc;
}

static value_t zip(const value_t* args, vm_t* vm, bool by)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t left = args[0];
    value_t right = args[1];
    value_t f = by ? args[2] : value_nil;
    CHECK_SEQ(left);
    CHECK_SEQ(right);
    int64_t n_left = seq_count(left);
    int64_t n_right = seq_count(right);
    value_arr zs;
    if (!values_init(&zs, n_left < n_right ? n_left : n_right, a))
        return OOM;

    cursor_t l = cursor(left);
    cursor_t r = cursor(right);
    value_t x, y;
    value_t err = value_nil;
    while (!failed(err) && cursor_next(&l, &x, a)) {
        if (!cursor_next(&r, &y, a)) {
            value_free(&x, a);
            break;
        }
        value_t pair[] = { x, y };
        value_t z = by ? vm_call_value(vm, f, pair, 2) : value_init_tuple(pair, 2, a);
        value_free(&x, a);
        value_free(&y, a);
        if (failed(z))
            err = z;
        else
            zs.arr[zs.size++] = z;
    }
    if (!failed(err) && (l.oom || r.oom))
        err = OOM;
    if (failed(err)) {
        value_arr_deinit(&zs, a);
        return err;
    }
    return wrap_list(zs, a);
}

/**
 * Finds the first x whose f(x) truthiness is `want`, and puts it in *hit unless hit is NULL.
 */
static value_t search(value_t seq, value_t f, bool want, bool* found, value_t* hit, vm_t* vm)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    *found = false;
    CHECK_SEQ(seq);
    cursor_t it = cursor(seq);
    value_t x;
    while (cursor_next(&it, &x, a)) {
        value_t r = vm_call_value(vm, f, &x, 1);
        if (failed(r)) {
            value_free(&x, a);
            return r;
        }
        bool truthy = value_is_truthy(r);
        value_free(&r, a);
        if (truthy == want) {
            *found = true;
            if (hit != NULL)
                *hit = x;
            else
                value_free(&x, a);
            return value_nil;
        }
        value_free(&x, a);
    }
    return it.oom ? OOM : value_nil;
}

static value_t slice(const value_t* args, vm_t* vm, bool take)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t count = args[1];
    CHECK_SEQ(seq);
    CHECK_NUMBER(count);

    // clamp first: casting nan or an out of range double is undefined
    int64_t len = seq_count(seq);
    int64_t k = !(count.number > 0) ? 0 : count.number >= (double)len ? len : (int64_t)count.number;
    if (IS_STR(seq)) {
        sv_str_t s = AS_STR(seq);
        return value_init_str(take ? str_slice(s, 0, k) : str_slice(s, k, len), a);
    }
    list_t l = AS_LIST(seq);
    return wrap_ll(take ? ll_slice(l, 0, k, a) : ll_slice(l, k, len, a), a);
}

/**
 * Compares two numbers, or two strings bytewise.
 */
static int order(value_t x, value_t y)
{
    if (IS_NUMBER(x))
        return (x.number > y.number) - (x.number < y.number);
    sv_str_t s = AS_STR(x), t = AS_STR(y);
    int c = memcmp(s.chars, t.chars, s.size < t.size ? s.size : t.size);
    if (c != 0)
        return c > 0 ? 1 : -1;
    return (s.size > t.size) - (s.size < t.size);
}

static bool orderable(value_t first, value_t k)
{
    return (IS_NUMBER(first) && IS_NUMBER(k)) || (IS_STR(first) && IS_STR(k));
}

/**
 * f(x), or x when there's no f. Errors unless it's a number or string like `first`.
 */
static value_t key_of(vm_t* vm, const value_t* f, value_t x, const value_t* first)
{
    value_t k = f == NULL ? value_borrow(x) : vm_call_value(vm, *f, &x, 1);
    if (failed(k))
        return k;
    value_t like = first == NULL ? k : *first;
    if (orderable(like, k))
        return k;
    value_t err = bad_arg_type_error(k, IS_STR(like) ? VALUE_OBJ : VALUE_NUMBER, OBJ_STR, &vm->ctx);
    value_free(&k, vm->ctx.alloc);
    return err;
}

/**
 * The element with the smallest key, or the largest. Ties keep the first.
 */
static value_t min_max(value_t seq, vm_t* vm, const value_t* f, bool largest)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    CHECK_SEQ(seq);
    value_t best = value_nil;
    value_t best_key = value_nil;
    bool has_best = false;

    cursor_t it = cursor(seq);
    value_t x;
    while (cursor_next(&it, &x, a)) {
        value_t k = key_of(vm, f, x, has_best ? &best_key : NULL);
        if (failed(k)) {
            value_free(&x, a);
            value_free(&best, a);
            best = k;
            break;
        }
        int c = has_best ? order(k, best_key) : 0;
        if (!has_best || (largest ? c > 0 : c < 0)) {
            value_free(&best, a);
            value_free(&best_key, a);
            best = x;
            best_key = k;
            has_best = true;
        } else {
            value_free(&x, a);
            value_free(&k, a);
        }
    }
    value_free(&best_key, a);
    if (it.oom) {
        value_free(&best, a);
        return OOM;
    }
    return best;
}

static int keyed_cmp(const void* l, const void* r)
{
    return order(((const keyed_t*)l)->key, ((const keyed_t*)r)->key);
}

static value_t sort(value_t seq, vm_t* vm, const value_t* f)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    CHECK_SEQ(seq);
    int64_t n = seq_count(seq);
    value_arr sorted;
    bool ok = values_init(&sorted, n, a);
    sv_vec_t(keyed_t) ks = sv_vec_init_capacity(keyed_t, n, a);
    value_t err = ok && (ks.arr != NULL || n == 0) ? value_nil : OOM;

    cursor_t it = cursor(seq);
    value_t x;
    while (!failed(err) && cursor_next(&it, &x, a)) {
        value_t k = key_of(vm, f, x, ks.size > 0 ? &ks.arr[0].key : NULL);
        if (failed(k)) {
            value_free(&x, a);
            err = k;
        } else {
            ks.arr[ks.size++] = (keyed_t){ .key = k, .value = x };
        }
    }
    if (!failed(err) && it.oom)
        err = OOM;

    if (!failed(err))
        stable_sort(ks.arr, ks.size, sizeof(keyed_t), keyed_cmp);
    for (int64_t i = 0; i < ks.size; i++) {
        value_free(&ks.arr[i].key, a);
        if (failed(err))
            value_free(&ks.arr[i].value, a);
        else
            sorted.arr[sorted.size++] = ks.arr[i].value;
    }
    sv_vec_deinit(&ks, a);
    if (failed(err)) {
        value_arr_deinit(&sorted, a);
        return err;
    }
    return wrap_like(seq, sorted, a);
}

static value_t fold_numbers(value_t seq, vm_t* vm, bool product)
{
    const sv_allocator_t* a = vm->ctx.alloc;
    CHECK_SEQ(seq);
    double acc = product ? 1 : 0;
    cursor_t it = cursor(seq);
    value_t x;
    while (cursor_next(&it, &x, a)) {
        if (!IS_NUMBER(x)) {
            value_t err = bad_arg_type_error(x, VALUE_NUMBER, OBJ_LIST, &vm->ctx);
            value_free(&x, a);
            return err;
        }
        acc = product ? acc * x.number : acc + x.number;
    }
    return it.oom ? OOM : NUMBER(acc);
}

value_t ntvl_map(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return map(args, vm, false); }
value_t ntvl_mapi(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return map(args, vm, true); }
value_t ntvl_filter(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return filter(args, vm, false); }
value_t ntvl_filteri(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return filter(args, vm, true); }
value_t ntvl_reduce(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 3); return reduce(args, vm, false); }
value_t ntvl_reducei(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 3); return reduce(args, vm, true); }
value_t ntvl_zip(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return zip(args, vm, false); }
value_t ntvl_zip_by(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 3); return zip(args, vm, true); }
value_t ntvl_take(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return slice(args, vm, true); }
value_t ntvl_skip(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 2); return slice(args, vm, false); }
value_t ntvl_min(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 1); return min_max(args[0], vm, NULL, false); }
value_t ntvl_max(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 1); return min_max(args[0], vm, NULL, true); }
value_t ntvl_sort(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 1); return sort(args[0], vm, NULL); }
value_t ntvl_sum(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 1); return fold_numbers(args[0], vm, false); }
value_t ntvl_mul(const value_t* args, uint8_t n, vm_t* vm) { assert(n == 1); return fold_numbers(args[0], vm, true); }

// args may move while f runs, so copy f first

value_t ntvl_min_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    value_t f = args[1];
    return min_max(args[0], vm, &f, false);
}

value_t ntvl_max_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    value_t f = args[1];
    return min_max(args[0], vm, &f, true);
}

value_t ntvl_sort_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    value_t f = args[1];
    return sort(args[0], vm, &f);
}

value_t ntvl_any(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    bool found;
    value_t err = search(args[0], args[1], true, &found, NULL, vm);
    return failed(err) ? err : (found ? value_true : value_false);
}

value_t ntvl_all(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    bool found_falsy;
    value_t err = search(args[0], args[1], false, &found_falsy, NULL, vm);
    return failed(err) ? err : (found_falsy ? value_false : value_true);
}

value_t ntvl_find_by(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    bool found;
    value_t hit;
    value_t err = search(args[0], args[1], true, &found, &hit, vm);
    if (failed(err))
        return err;
    return found ? hit : value_nil;
}

value_t ntvl_at(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    value_t seq = args[0];
    value_t index = args[1];
    CHECK_SEQ(seq);
    CHECK_NUMBER(index);

    // check first: casting nan or an out of range double is undefined
    int64_t len = seq_count(seq);
    if (!(index.number >= (double)-len && index.number < (double)len))
        return value_nil;
    int64_t i = (int64_t)index.number;
    if (IS_STR(seq)) {
        sv_str_t c = str_get(AS_STR(seq), i);
        return c.size == 0 ? value_nil : value_init_str(c, vm->ctx.alloc);
    }
    sv_opt_t(value_t) v = ll_get(AS_LIST(seq), i);
    return v.is_some ? value_borrow(v.value) : value_nil;
}

value_t ntvl_count(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);
    CHECK_SEQ(args[0]);
    return NUMBER(seq_count(args[0]));
}

value_t ntvl_prepend(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t x = args[1];
    CHECK_SEQ(seq);
    if (IS_LIST(seq))
        return wrap_ll(ll_prepend(AS_LIST(seq), x, a), a);
    if (!IS_STR(x))
        return bad_arg_type_error(x, VALUE_OBJ, OBJ_STR, &vm->ctx);
    return str_concat(x, seq, a);
}

value_t ntvl_concat(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t left = args[0];
    value_t right = args[1];
    CHECK_SEQ(left);
    if (IS_LIST(left) ? !IS_LIST(right) : !IS_STR(right))
        return bad_arg_type_error(right, VALUE_OBJ, IS_LIST(left) ? OBJ_LIST : OBJ_STR, &vm->ctx);
    if (IS_LIST(left))
        return wrap_ll(ll_add(AS_LIST(left), AS_LIST(right), a), a);
    return str_concat(left, right, a);
}

value_t ntvl_find(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t v = args[1];
    CHECK_SEQ(seq);
    cursor_t it = cursor(seq);
    value_t x;
    for (int64_t i = 0; cursor_next(&it, &x, a); i++) {
        bool eq = value_eql(x, v);
        value_free(&x, a);
        if (eq)
            return NUMBER(i);
    }
    return it.oom ? OOM : value_nil;
}

value_t ntvl_each(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    const sv_allocator_t* a = vm->ctx.alloc;
    value_t seq = args[0];
    value_t f = args[1];
    CHECK_SEQ(seq);
    cursor_t it = cursor(seq);
    value_t x;
    while (cursor_next(&it, &x, a)) {
        value_t r = vm_call_value(vm, f, &x, 1);
        value_free(&x, a);
        if (failed(r))
            return r;
        value_free(&r, a);
    }
    return it.oom ? OOM : value_nil;
}

#undef CHECK_NUMBER
#undef CHECK_SEQ
#undef OOM
#undef NUMBER
