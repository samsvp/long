#ifndef SV_TESTS_STR_ITER_H
#define SV_TESTS_STR_ITER_H

#include "../src/std/test.h"
#include "../src/std/allocator_std.h"
#include "../src/value.h"
#include "../src/obj.h"

static inline void sv_test_str_iter(sv_testing_t* t)
{
   value_t s = value_init_str(sv_str_init("aé😀"), &sv_gpa);
   value_t it = value_init_iter(s, &sv_gpa);

   iterable_t* iterable = &AS_ITER(it);
   const char* expected[] = { "a", "é", "😀" };
   for (int i = 0; i < 3; i++) {
      sv_opt_t(value_t) c = iterable_next(iterable, &sv_gpa);
      sv_test_run(t, c.is_some && IS_STR(c.value));
      sv_test_run(t, c.is_some && sv_str_comp(AS_STR(c.value), sv_str_init(expected[i])));
      if (c.is_some)
         value_free(&c.value, &sv_gpa);
   }
   sv_test_run(t, !iterable_next(iterable, &sv_gpa).is_some);

   value_free(&it, &sv_gpa);
   value_free(&s, &sv_gpa);
}

#endif
