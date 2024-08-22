/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#include "types.h"
#include "crypt.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <syslog.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/err.h>

int hash_pin(const pin_source_t pin, pin_hash_t output) {
        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (mdctx == NULL) {
                return HASH_ERR_CTX;
        }

        int err = 0;
        if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
                err = HASH_ERR_DIGEST_INIT;
                goto HASH_PIN_RET;
        }

        if (EVP_DigestUpdate(mdctx, pin, PIN_SOURCE_LEN) != 1) {
                err = HASH_ERR_DIGEST_UPDATE;
                goto HASH_PIN_RET;
        }

        unsigned int len = SHA256_DIGEST_LENGTH;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        memset(hash, 0, SHA256_DIGEST_LENGTH);
        if (EVP_DigestFinal_ex(mdctx, hash, &len) != 1) {
                err = HASH_ERR_DIGEST_FINAL;
                goto HASH_PIN_RET;
        }

        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                sprintf((char *)output + (i * 2), "%02x", hash[i]);
        }

HASH_PIN_RET:
        EVP_MD_CTX_free(mdctx);
        return err;
}

