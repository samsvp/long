#include "tuple.h"
#include "../value.h"
#include "../obj.h"

tuple_t tuple_init(const value_t* values, uint8_t size, const sv_allocator_t* a)
{
    value_t* items = sv_malloc(a, size * sizeof(value_t));
    if (items == NULL)
        return (tuple_t){0};

    for (uint8_t i = 0; i < size; i++)
        items[i] = value_borrow(values[i]);

    return (tuple_t){ .items = items, .size = size };
}

void tuple_deinit(tuple_t* t, const sv_allocator_t* a)
{
    for (uint8_t i = 0; i < t->size; i++)
        value_free(&t->items[i], a);
    sv_free(a, t->items);
}

bool tuple_eql(tuple_t t1, tuple_t t2)
{
    if (t1.size != t2.size)
        return false;

    for (uint8_t i = 0; i < t1.size; i++)
        if (!value_eql(t1.items[i], t2.items[i]))
            return false;

    return true;
}

sv_opt_t(value_t) tuple_get(tuple_t tuple, int64_t i)
{
    if (i < 0)
        i += tuple.size;
    if (i < 0 || i >= tuple.size)
        return sv_opt_none_t(value_t);
    return sv_opt_some_t(value_t, tuple.items[i]);
}

tuple_iter_t tuple_iter_init(value_t v)
{
    return (tuple_iter_t){ .root = sv_rc_borrow(v.obj) };
}

void tuple_iter_deinit(tuple_iter_t* it, const sv_allocator_t* a)
{
    sv_rc_deinit(&it->root, a);
}

static int64_t iterable_tuple_len(const void* self)
{
    const tuple_iter_t* it = self;
    return it->root.cell->value.tuple.size - it->i;
}

static sv_opt_t(value_t) iterable_tuple_next(void* self, const sv_allocator_t* a)
{
    (void)a;
    tuple_iter_t* it = self;
    tuple_t t = it->root.cell->value.tuple;
    return it->i < t.size ? sv_opt_some_t(value_t, value_borrow(t.items[it->i++])) : sv_opt_none_t(value_t);
}

static void iterable_tuple_deinit(void* self, const sv_allocator_t* a)
{
    tuple_iter_deinit(self, a);
}

static const iterable_vtable_t tuple_iterable_vtable = {
    .name = "tuple iterator",
    .len = iterable_tuple_len,
    .next = iterable_tuple_next,
    .deinit = iterable_tuple_deinit,
};

iterable_t tuple_iterable_init(tuple_iter_t* it)
{
    return (iterable_t){ .vtable = &tuple_iterable_vtable, .self = it };
}
