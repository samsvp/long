#ifndef LONG_MOD_MATH
#define LONG_MOD_MATH

#include "../common.h"

#define NUMBER(x) (value_t){ .kind = VALUE_NUMBER, .number = (x) }

#define CHECK_NUMBER(v) do {                                                   \
    if (!IS_NUMBER(v))                                                         \
        return bad_arg_type_error(v, VALUE_NUMBER, OBJ_LIST, &vm->ctx);        \
} while (0)

/**
 * abs(x) Absolute value of x.
 */
value_t ntvm_abs(const value_t*, uint8_t, vm_t*);
/**
 * sin(x) Sine of x (measured in radians)
 */
value_t ntvm_sin(const value_t*, uint8_t, vm_t*);
/**
 * cos(x) Cosine of x (measured in radians)
 */
value_t ntvm_cos(const value_t*, uint8_t, vm_t*);
/**
 * tan(x) Tangent of x (measured in radians)
 */
value_t ntvm_tan(const value_t*, uint8_t, vm_t*);
/**
 * acos(x) Arc cosine of x
 */
value_t ntvm_acos(const value_t*, uint8_t, vm_t*);
/**
 * asin(x) Arc sine of x
 */
value_t ntvm_asin(const value_t*, uint8_t, vm_t*);
/**
 * atan(x) Arc tangent of x
 */
value_t ntvm_atan(const value_t*, uint8_t, vm_t*);
/**
 * ceil(x) Ceiling of x, the smallest integer greater than or equal to x
 */
value_t ntvm_ceil(const value_t*, uint8_t, vm_t*);
/**
 * floor(x) Floor of x, the largest integer less than or equal to x
 */
value_t ntvm_floor(const value_t*, uint8_t, vm_t*);
/**
 * trunc(x) Truncates x to the nearest integer toward 0
 */
value_t ntvm_trunc(const value_t*, uint8_t, vm_t*);
/**
 * modf(x) Fractional and integer parts of x as a tuple, both with the sign of x
 */
value_t ntvm_modf(const value_t*, uint8_t, vm_t*);
/**
 * min(x, y) Smallest of x and y
 */
value_t ntvm_min(const value_t*, uint8_t, vm_t*);
/**
 * max(x, y) Largest of x and y
 */
value_t ntvm_max(const value_t*, uint8_t, vm_t*);
/**
 * mod(x, y) Remainder of x / y with the sign of x, like fmod
 */
value_t ntvm_mod(const value_t*, uint8_t, vm_t*);
/**
 * pow(x, y) x raised to the power y
 */
value_t ntvm_pow(const value_t*, uint8_t, vm_t*);
/**
 * exp(x) e raised to the power x
 */
value_t ntvm_exp(const value_t*, uint8_t, vm_t*);
/**
 * log(x) Natural logarithm of x
 */
value_t ntvm_log(const value_t*, uint8_t, vm_t*);
/**
 * sqrt(x) Square root of x
 */
value_t ntvm_sqrt(const value_t*, uint8_t, vm_t*);
/**
 * degrees(x) Convert angle x from radians to degrees
 */
value_t ntvm_degrees(const value_t*, uint8_t, vm_t*);
/**
 * radians(x) Convert angle x from degrees to radians
 */
value_t ntvm_radians(const value_t*, uint8_t, vm_t*);
/**
 * isinf(x) True if x is a positive or negative infinity
 */
value_t ntvm_isinf(const value_t*, uint8_t, vm_t*);
/**
 * isnan(x) True if x is a NaN (not a number)
 */
value_t ntvm_isnan(const value_t*, uint8_t, vm_t*);
/**
 * pi() The constant π = 3.141592…
 */
value_t ntvm_pi(const value_t*, uint8_t, vm_t*);
/**
 * e() The constant e = 2.718281…
 */
value_t ntvm_e(const value_t*, uint8_t, vm_t*);
/**
 * inf() Floating-point positive infinity
 */
value_t ntvm_inf(const value_t*, uint8_t, vm_t*);
/**
 * nan() Floating-point "not a number" (NaN) value
 */
value_t ntvm_nan(const value_t*, uint8_t, vm_t*);

#endif
