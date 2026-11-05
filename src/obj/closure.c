#include "closure.h"
#include "../value.h"
#include "../obj.h"

void cls_deinit(closure_t* cls, const sv_allocator_t* a)
{
    sv_vec_foreach(value_t, v, &cls->upvalues)
        value_free(&v, a);
    sv_vec_deinit(&cls->upvalues, a);
}

void clsg_deinit(closure_group_t* g, const sv_allocator_t* a)
{
    sv_vec_foreach(value_t, v, &g->upvalues)
        value_free(&v, a);
    sv_vec_deinit(&g->upvalues, a);
}

void clsm_deinit(closure_member_t* m, const sv_allocator_t* a)
{
    sv_rc_deinit(&m->group, a);
}
