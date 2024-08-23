/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _USERS_H
#define _USERS_H

#include "types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>


typedef struct user user_t;

struct users;
typedef struct users users_t;

typedef enum {
        // io errors
        ERR_USERS_OPEN = 1,
        ERR_USERS_ACCES,
        ERR_USERS_NOT_FOUND,
        ERR_USERS_READ,
        ERR_USERS_WRITE,
        ERR_USERS_CLOSE,

        ERR_USERS_INVALID_FORMAT,
        ERR_USERS_USER_NOT_FOUND,
} users_error_t;

users_t* users_new(const int cap);

// load users from file storage.
int users_load(users_t *storage, const char* filepath);

int users_dump(users_t *storage, const char* filepath);

int users_find(users_t *storage,
               const char *username,
               user_t *user);

int users_update(users_t *storage,
                 const char *username,
                 const pin_hash_t pin_hash);

int users_remove(users_t *storage,
                 const char *username);

user_t* user_new();
void user_free(user_t *user);

struct user_iterator;
typedef struct user_iterator user_iterator_t;

user_iterator_t* users_iterate(users_t *storage);

bool users_iterator_next(user_iterator_t *iter, user_t *out);

void users_free(users_t *storage);

typedef enum {
        USER_PRINT_USERNAME =   0x0001,
        USER_PRINT_PINHASH =    0x0010,
} user_print_fmt;

int user_print(FILE *out, user_t *user, user_print_fmt format);

bool user_check_pin(user_t *user, pin_hash_t pin_hash);

const char* user_get_name(user_t *user);

#endif
