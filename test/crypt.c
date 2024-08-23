#include "test.h"
#include "../src/lib/crypt.h"


testfunc(hash_pin) {
        (void) state;  // Unused variable

        pin_source_t pin = {1, 2, 3, 4};
        pin_hash_t expect = "9f64a747e1b97f131fabb6b447296c9b6f0201e79fb3c5356e6c77e89b6a806a";

        pin_hash_t output;
        int ret = hash_pin(pin, output);
        assert_int_equal(ret, 0);
        assert_memory_equal(output, expect, sizeof(pin_hash_t));
}
