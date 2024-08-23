#include "test.h"
#include <cmocka.h>

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_users_update),
        cmocka_unit_test(test_users_remove),
        cmocka_unit_test(test_users_iterate),
        cmocka_unit_test(test_hash_pin),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
