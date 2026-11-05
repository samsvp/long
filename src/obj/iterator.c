#include "iterator.h"
#include "../value.h"
#include "../obj.h"

/* Initializes the iterator in storage, borrowing the value when asked to. */
static iterable_t init_in(value_t from, iter_storage_t* s, bool borrow)
{
    if (VALUE_KIND(from) != VALUE_OBJ)
        return (iterable_t){0};

    switch (AS_OBJ(from).cell->value.kind) {
        case OBJ_LIST:
            s->list = borrow ? ll_iter_init(AS_LIST(from)) : ll_iter_init_no_borrow(AS_LIST(from));
            return ll_iterable_init(&s->list);
        case OBJ_STR:
            s->str = borrow ? str_iter_init(&AS_STR(from)) : str_iter_init_no_borrow(&AS_STR(from));
            return str_iterable_init(&s->str);
        case OBJ_MAP:
            s->map = borrow ? map_iter_init(AS_MAP(from)) : map_iter_init_no_borrow(AS_MAP(from));
            return map_iterable_init(&s->map);
        case OBJ_TUPLE:
            s->tuple = borrow ? tuple_iter_init(from) : tuple_iter_init_no_borrow(from);
            return tuple_iterable_init(&s->tuple);
        case OBJ_ACTOR:
        case OBJ_USER_VALUE:
        case OBJ_ITER:
        case OBJ_RECORD:
        case OBJ_ERR:
        case OBJ_NATIVE_FN:
        case OBJ_CLOSURE:
        case OBJ_CLOSURE_MEMBER:
            break;
    }
    return (iterable_t){0};
}

iterable_t iter_init_no_borrow(value_t from, iter_storage_t* storage)
{
    return init_in(from, storage, false);
}

iterable_t iter_init(value_t from, const sv_allocator_t* a)
{
    iter_storage_t* s = sv_malloc(a, sizeof(iter_storage_t));
    if (s == NULL)
        return (iterable_t){0};
    iterable_t it = init_in(from, s, true);
    if (it.self == NULL)
        sv_free(a, s);
    return it;
}

void iter_deinit(iterable_t* it, const sv_allocator_t* a)
{
    iterable_deinit(it, a);
    sv_free(a, it->self);
}
