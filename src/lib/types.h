/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#define PIN_HASH_LEN (SHA256_DIGEST_LENGTH * 2)  // 64 hex chars

typedef uint8_t pin_hash_t[PIN_HASH_LEN];

#define PIN_SOURCE_LEN 4

typedef uint8_t pin_source_t[PIN_SOURCE_LEN];

#endif
