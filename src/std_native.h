/**
 * Lóng lang standard library native functions. The VM already checks for arity before calling the function.
 */

#ifndef LONG_STD_NATIVE_H
#define LONG_STD_NATIVE_H

#include <stdint.h>
#include "common.h"
#include "value.h"
#include "obj.h"

value_t bad_arg_type_error(value_t, value_kind expected_k, obj_kind expected_o, const vm_ctx_t*);

void init_rand(void);

/**
 * Prints one value to stdout.
 */
value_t ntv_print(const value_t*, uint8_t, vm_t*);
/**
 * printf(str, list) Prints the list of values to stdout given the format string.
 * Use %v to print a value.
 */
value_t ntv_printf(const value_t*, uint8_t, vm_t*);
/**
 * printf(str, list) Prints the list of values to stdout given the format string.
 * Use %v to print a value. Adds a new line at the end.
 */
value_t ntv_printfln(const value_t*, uint8_t, vm_t*);
/**
 * Prints one value to stdout and adds a new line.
 */
value_t ntv_println(const value_t*, uint8_t, vm_t*);
/**
 * Returns the length of an iterable.
 */
value_t ntv_len(const value_t*, uint8_t, vm_t*);
/**
 * Returns a random floating point number between min (the first value, inclusive) and max (the second value, inclusive).
 */
value_t ntv_randf(const value_t*, uint8_t, vm_t*);
/**
 * Returns a random integer between min (the first value, inclusive) and max (the second value, inclusive).
 * This seeds srand with time(NULL). Define RAND_SEED_DEFINED to skip srand initialization.
 */
value_t ntv_randi(const value_t*, uint8_t, vm_t*);
/**
 * Returns the value as an int
 */
value_t ntv_int(const value_t*, uint8_t, vm_t*);

#endif
