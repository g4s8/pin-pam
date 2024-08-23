#include "test.h"
#include "../src/lib/users.h"

testfunc(users_update) {
        (void) state;  // Unused variable

        pin_hash_t pin1 = {1};
        pin_hash_t pin2 = {2};

        users_t *users = users_new(4);
        users_update(users, "John", pin1);
        users_update(users, "Jane", pin2);

        user_t *u1 = user_new();
        user_t *u2 = user_new();
        user_t *u3 = user_new();

        int ret1 = users_find(users, "John", u1);
        int ret2 = users_find(users, "Jane", u2);
        int ret3 = users_find(users, "Alice", u3);

        assert_int_equal(ret1, 0);
        assert_int_equal(ret2, 0);
        assert_int_equal(ret3, ERR_USERS_USER_NOT_FOUND);

        bool c1 = user_check_pin(u1, pin1);
        bool c2 = user_check_pin(u2, pin2);

        assert_true(c1);
        assert_true(c2);

        user_free(u1);
        user_free(u2);
        user_free(u3);
        users_free(users);
}

void test_users_remove(void **state) {
        (void) state;  // Unused variable

        pin_hash_t pin = {1};

        users_t *users = users_new(4);
        users_update(users, "John", pin);
        users_update(users, "Jane", pin);

        user_t *u = user_new();

        int ret1 = users_find(users, "John", u);
        int ret2 = users_find(users, "Jane", u);

        assert_int_equal(ret1, 0);
        assert_int_equal(ret2, 0);

        users_remove(users, "John");
        int ret4 = users_find(users, "John", u);
        int ret5 = users_find(users, "Jane", u);

        assert_int_equal(ret4, ERR_USERS_USER_NOT_FOUND);
        assert_int_equal(ret5, 0);

        user_free(u);
        users_free(users);
}

void test_users_iterate(void **state) {
        (void) state;  // Unused variable

        pin_hash_t pin1 = {1};
        pin_hash_t pin2 = {2};

        users_t *users = users_new(4);
        users_update(users, "John", pin1);
        users_update(users, "Jane", pin2);

        user_t *u = user_new();

        user_iterator_t *iter = users_iterate(users);
        bool ok;

        ok = users_iterator_next(iter, u);
        assert_true(ok);
        const char* name1 = user_get_name(u);
        assert_string_equal(name1, "John");
        free((void*)name1);

        ok = users_iterator_next(iter, u);
        assert_true(ok);
        const char* name2 = user_get_name(u);
        assert_string_equal(name2, "Jane");
        free((void*)name2);

        ok = users_iterator_next(iter, u);
        assert_false(ok);

        user_free(u);
        users_free(users);
}
