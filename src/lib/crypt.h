/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _CRYPT_H
#define _CRYPT_H

#include "types.h"

typedef enum {
        HASH_ERR_CTX = 1,
        HASH_ERR_DIGEST_INIT,
        HASH_ERR_DIGEST_UPDATE,
        HASH_ERR_DIGEST_FINAL,
} hash_error_t;

int hash_pin(const pin_source_t pin, pin_hash_t output);

#endif
