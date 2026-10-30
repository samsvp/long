#include "math.h"
#include "../value.h"
#include "../std_native.h"
#include "../vm.h"
#include <assert.h>
#include <math.h>

static const double PI = 3.14159265358979323846;
static const double E = 2.71828182845904523536;

// x is the argument in expr
#define UNARY(name, expr)                                                      \
value_t ntvm_##name(const value_t* args, uint8_t n, vm_t* vm)                  \
{                                                                              \
    assert(n == 1);                                                            \
    CHECK_NUMBER(args[0]);                                                     \
    double x = AS_NUMBER(args[0]);                                             \
    return (expr);                                                             \
}

// x and y are the arguments in expr
#define BINARY(name, expr)                                                     \
value_t ntvm_##name(const value_t* args, uint8_t n, vm_t* vm)                  \
{                                                                              \
    assert(n == 2);                                                            \
    CHECK_NUMBER(args[0]);                                                     \
    CHECK_NUMBER(args[1]);                                                     \
    double x = AS_NUMBER(args[0]);                                             \
    double y = AS_NUMBER(args[1]);                                             \
    return NUMBER(expr);                                                       \
}

#define CONSTANT(name, c)                                                      \
value_t ntvm_##name(const value_t* args, uint8_t n, vm_t* vm)                  \
{                                                                              \
    (void)args; (void)vm;                                                      \
    assert(n == 0);                                                            \
    return NUMBER(c);                                                          \
}

UNARY(abs, NUMBER(fabs(x)))
UNARY(sin, NUMBER(sin(x)))
UNARY(cos, NUMBER(cos(x)))
UNARY(tan, NUMBER(tan(x)))
UNARY(acos, NUMBER(acos(x)))
UNARY(asin, NUMBER(asin(x)))
UNARY(atan, NUMBER(atan(x)))
UNARY(ceil, NUMBER(ceil(x)))
UNARY(floor, NUMBER(floor(x)))
UNARY(trunc, NUMBER(trunc(x)))
UNARY(exp, NUMBER(exp(x)))
UNARY(log, NUMBER(log(x)))
UNARY(sqrt, NUMBER(sqrt(x)))
UNARY(degrees, NUMBER(x * 180.0 / PI))
UNARY(radians, NUMBER(x * PI / 180.0))
UNARY(isinf, isinf(x) ? value_true : value_false)
UNARY(isnan, isnan(x) ? value_true : value_false)

BINARY(min, fmin(x, y))
BINARY(max, fmax(x, y))
BINARY(mod, fmod(x, y))
BINARY(pow, pow(x, y))

CONSTANT(pi, PI)
CONSTANT(e, E)
CONSTANT(inf, INFINITY)
CONSTANT(nan, NAN)

value_t ntvm_modf(const value_t* args, uint8_t n, vm_t* vm)
{
    assert(n == 1);
    CHECK_NUMBER(args[0]);

    double whole = 0;
    double frac = modf(AS_NUMBER(args[0]), &whole);
    value_t parts[] = { NUMBER(frac), NUMBER(whole) };
    return value_init_tuple(parts, 2, vm->ctx.alloc);  // NULL cell on OOM, see CALL_NATIVE
}

#undef CONSTANT
#undef BINARY
#undef UNARY
#undef CHECK_NUMBER
#undef NUMBER
