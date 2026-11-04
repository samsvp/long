#include "actor.h"
#include "../vm.h"
#include "../obj/tuple.h"

static value_t function_not_defined(const char* fn, const actor_t* actor, vm_t* vm)
{
    return value_init_err(
        error_fmt(VM_ERR_NOT_IMPLEMENTED, vm->ctx.line, vm->ctx.alloc,
                 "Actor '%s' has no '%s' function", actor->vtable->name, fn),
        vm->ctx.alloc
    );
}

static value_t ret_not_tuple(value_t val, const actor_t* actor, vm_t* vm)
{
    sv_str_t str = value_to_str(val, &vm->ctx);
    if (str.chars == NULL)
        str = sv_str_init("");

    value_t err = value_init_err(
        error_fmt(VM_ERR_WRONG_TYPE, vm->ctx.line, vm->ctx.alloc,
                 "Actor '%s' call function must return a size two tuple, (ret, new_state). Got: %.*s",
                 actor->vtable->name, (int)str.size, str.chars),
        vm->ctx.alloc
    );

    if (str.size > 0)
        sv_str_deinit(&str, vm->ctx.alloc);
    return err;
}

static value_t busy_error(const actor_t* actor, vm_t* vm)
{
    return value_init_err(
        error_fmt(VM_ERR_OP_UNSUPPORTED_ARGS, vm->ctx.line, vm->ctx.alloc,
                  "Actor '%s' cannot be updated while it is updating", actor->vtable->name),
        vm->ctx.alloc);
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

static value_t call_busy(actor_t* actor, value_t arg, value_t function,
                         const char* fun_name, vm_t* vm)
{
    if (IS_NIL(function))
        return function_not_defined(fun_name, actor, vm);
    if (actor->busy)
        return busy_error(actor, vm);

    value_t args[2] = { actor->state, arg };
    actor->busy = true;
    value_t new_state = vm_call_value(vm, function, args, 2);
    actor->busy = false;

    return new_state;
}

value_t actor_get(value_t actor_val, value_t arg, vm_t* vm)
{
    actor_t* actor = &AS_ACTOR(actor_val);
    return call_busy(actor, arg, actor->vtable->get_fn, "get", vm);
}

value_t actor_update(value_t actor_val, value_t arg, vm_t* vm)
{
    actor_t* actor = &AS_ACTOR(actor_val);
    value_t new_state = call_busy(actor, arg, actor->vtable->update_fn, "update", vm);
    if (IS_ERR(new_state))
        return new_state;

    value_free(&actor->state, vm->ctx.alloc);
    actor->state = new_state;
    return value_borrow(actor_val);
}

value_t actor_call(value_t actor_val, value_t arg, vm_t* vm)
{
    actor_t* actor = &AS_ACTOR(actor_val);
    value_t new_state = call_busy(actor, arg, actor->vtable->call_fn, "call", vm);
    if (IS_ERR(new_state))
        return new_state;

    if (!IS_TUPLE(new_state) || AS_TUPLE(new_state).size != 2) {
        value_t ret = ret_not_tuple(new_state, actor, vm);
        value_free(&new_state, vm->ctx.alloc);
        return ret;
    }

    tuple_t tuple = AS_TUPLE(new_state);

    value_free(&actor->state, vm->ctx.alloc);
    actor->state = value_borrow(tuple.items[0]);
    value_t ret = value_borrow(tuple.items[1]);
    value_free(&new_state, vm->ctx.alloc);
    return ret;
}
