#include "actor.h"
#include "../value.h"
#include "../obj.h"
#include "../std_native.h"
#include "../vm.h"
#include <assert.h>

value_t ntva_get(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    if (!IS_ACTOR(args[0]))
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ACTOR, vm);
    return actor_get(args[0], args[1], vm);
}

value_t ntva_update(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    if (!IS_ACTOR(args[0]))
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ACTOR, vm);
    return actor_update(args[0], args[1], vm);
}

value_t ntva_call(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 2);
    if (!IS_ACTOR(args[0]))
        return bad_arg_type_error(args[0], VALUE_OBJ, OBJ_ACTOR, vm);
    return actor_call(args[0], args[1], vm);
}
