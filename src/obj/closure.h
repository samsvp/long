#ifndef LONG_CLOSURE_H
#define LONG_CLOSURE_H

#include "../std/vector.h"
#include "../std/rc.h"
#include "fn.h"

typedef struct {
    fn_t* function;
    sv_vec_t(value_t) upvalues;
} closure_t;


typedef struct {
    // non-owning view of the chunk's storage, in block order
    sv_vec_t(fn_t) members;
    // every member's captured upvalues, concatenated; each member's offset is
    // baked into its get_upvalue operands at compile time
    sv_vec_t(value_t) upvalues;
} closure_group_t;

sv_rc_def(closure_group_t);

typedef struct {
    sv_rc_t(closure_group_t) group;
    size_t index;
} closure_member_t;

static inline fn_t* cls_get_vm(closure_t cls)
{
    return cls.function;
}

void cls_deinit(closure_t*, const sv_allocator_t*);

void clsg_deinit(closure_group_t*, const sv_allocator_t*);

static inline fn_t* clsm_get_vm(closure_member_t m)
{
    return &m.group.cell->value.members.arr[m.index];
}

void clsm_deinit(closure_member_t*, const sv_allocator_t*);

#endif
