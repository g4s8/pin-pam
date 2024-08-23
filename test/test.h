#ifndef _TEST_H_
#define _TEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>

#define testfunc(name) void test_##name(void **state)

testfunc(users_update);
testfunc(users_remove);
testfunc(users_iterate);

testfunc(hash_pin);

#endif
