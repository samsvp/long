#include "actor.h"
#include "error.h"
#include "../vm.h"

static value_t function_not_defined(const char* fn, const actor_t* actor, vm_t* vm)
{
    return value_init_err(
        error_fmt(VM_ERR_NOT_IMPLEMENTED, vm->ctx.line, vm->ctx.alloc,
                 "Actor '%s' has no '%s' function", actor->vtable->name, fn),
        vm->ctx.alloc
    );
}

actor_t actor_init(value_t state, const actor_vtable_t* vtable)
{
    return (actor_t) {
        .state = state,
        .vtable = vtable,
    };
}

void actor_free(actor_t* actor, const sv_allocator_t* a)
{
    value_free(&actor->state, a);
}

value_t actor_get(value_t actor_val, value_t value, vm_t* vm)
{
    actor_t actor = AS_ACTOR(actor_val);
    if (IS_NIL(actor.vtable->get_fn))
        return function_not_defined("get", &actor, vm);

    value_t args[2] = { actor.state, value };
    return vm_call_value(vm, actor.vtable->get_fn, args, 2);
}

value_t actor_update(value_t actor_val, value_t value, vm_t* vm)
{
    actor_t* actor = &AS_ACTOR(actor_val);
    if (IS_NIL(actor->vtable->update_fn))
        return function_not_defined("update", actor, vm);
    if (actor->busy)
        return value_init_err(
            error_fmt(VM_ERR_OP_UNSUPPORTED_ARGS, vm->ctx.line, vm->ctx.alloc,
                      "Actor '%s' cannot be updated while it is updating", actor->vtable->name),
            vm->ctx.alloc);

    value_t args[2] = { actor->state, value };
    actor->busy = true;
    value_t new_state = vm_call_value(vm, actor->vtable->update_fn, args, 2);
    actor->busy = false;
    if (IS_ERR(new_state))
        return new_state;

    value_free(&actor->state, vm->ctx.alloc);
    actor->state = new_state;
    return value_borrow(actor_val);
}
