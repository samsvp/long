#include "iterator.h"
#include "../value.h"
#include "../obj.h"
#include "list.h"
#include "map.h"
#include "str.h"

iterable_t iter_init(value_t from, const sv_allocator_t* a)
{
    if (from.kind != VALUE_OBJ)
        return (iterable_t){0};

    switch (from.obj.cell->value.kind) {
        case OBJ_LIST: {
            ll_iter_t* it = sv_malloc(a, sizeof(ll_iter_t));
            if (it == NULL)
                break;
            *it = ll_iter_init(AS_LIST(from));
            return ll_iterable_init(it);
        }
        case OBJ_STR: {
            str_iter_t* it = sv_malloc(a, sizeof(str_iter_t));
            if (it == NULL)
                break;
            *it = str_iter_init(&AS_STR(from));
            return str_iterable_init(it);
        }
        case OBJ_MAP: {
            map_iter_t* it = sv_malloc(a, sizeof(map_iter_t));
            if (it == NULL)
                break;
            *it = map_iter_init(AS_MAP(from));
            return map_iterable_init(it);
        }
        case OBJ_USER_VALUE:
        case OBJ_ITER:
        case OBJ_RECORD:
        case OBJ_TUPLE:
        case OBJ_ERR:
        case OBJ_NATIVE_FN:
        case OBJ_CLOSURE:
        case OBJ_CLOSURE_MEMBER:
            break;
    }
    return (iterable_t){0};
}

void iter_deinit(iterable_t* it, const sv_allocator_t* a)
{
    iterable_deinit(it, a);
    sv_free(a, it->self);
}
