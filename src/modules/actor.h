#ifndef LONG_MOD_ACTOR
#define LONG_MOD_ACTOR

#include "../common.h"

/**
 * get(actor, msg) Asks the actor's get function about msg.
 */
value_t ntva_get(const value_t*, uint8_t, vm_t*);
/**
 * update(actor, msg) Replaces the actor's state with its update function's result. Returns the actor.
 */
value_t ntva_update(const value_t*, uint8_t, vm_t*);

#endif
