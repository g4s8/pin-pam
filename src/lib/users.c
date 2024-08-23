/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#define _GNU_SOURCE
#include "users.h"
#include "utils.h"

#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

// internal implementation

#define ERR_READ_EOF -101

struct user {
        const char              *username;
        const pin_hash_t        pin_hash;

        bool _allocated;
};

struct users {
        user_t  *users;
        size_t  ulen;
        size_t  ucap;
};

static int users_add(users_t *storage,
                const char *username,
                const pin_hash_t pin,
                const bool allocated);

static int users_resize(users_t *storage);

static int users_scan_line(FILE *file, char **username, pin_hash_t pin_hash);

static int user_print_line(FILE *file, const user_t *user);

static int user_copy(user_t *dst, const user_t *src);

// public interface

users_t* users_new(const int cap) {
        users_t *storage = malloc(sizeof(users_t));
        if (storage == NULL) {
                return NULL;
        }
        storage->users = NULL;
        storage->ulen = 0;
        storage->ucap = 0;
        if (cap > 0) {
                storage->ucap = cap;
                storage->users = malloc(cap * sizeof(user_t));
                if (storage->users == NULL) {
                        free(storage);
                        return NULL;
                }
        }
        return storage;
}

int users_load(users_t *storage, const char* filepath) {
        FILE *file = fopen(filepath, "r");
        if (file == NULL) {
                switch (errno) {
                        case ENOENT:
                                // file not found - no error
                                storage->ucap = 0;
                                storage->ulen = 0;
                                return 0;

                        ERRORS_CASE(EACCES, ERR_USERS_ACCES);
                        ERRORS_DEFAULT(ERR_USERS_OPEN);
                }
        }

        /*
         * User file format:
         * <username:string>:<pin_hash:binary>\n
         * <username:string>:<pin_hash:binary>\n
         * <username:string>:<pin_hash:binary>\n
         * EOF
         */
        int err = 0;

        while (!feof(file)) {
                char *username = NULL;
                pin_hash_t pin_hash;
                err = users_scan_line(file, &username, pin_hash);
                if (err != 0) {
                        break;
                }

                err = users_add(storage, username, pin_hash, true);
                if (err != 0) {
                        free(username);
                        break;
                }
        }

        if (err == ERR_READ_EOF) {
                err = 0;
        }

        if (err != 0) {
              goto USERS_LOAD_ERR;
        }

        int ferr = fclose(file);
        if (ferr != 0) {
                switch (errno) {
                        ERRORS_CASE(EACCES, ERR_USERS_ACCES);
                        ERRORS_DEFAULT(ERR_USERS_CLOSE);
                }
        }
        return 0;

USERS_LOAD_ERR:
        if (file != NULL) {
                fclose(file);
        }
        users_free(storage);
        return err;
}


int users_find(users_t *storage,
               const char *username,
               user_t *user) {
        for (size_t i = 0; i < storage->ulen; i++) {
                if (strcmp(storage->users[i].username, username) == 0) {
                        if (user != NULL) {
                                int err = user_copy(user, &storage->users[i]);
                                if (err != 0) {
                                        return err;
                                }
                        }
                        return 0;
                }
        }
        return ERR_USERS_USER_NOT_FOUND;
}

int users_dump(users_t *storage, const char* filepath) {
        // TODO: lock the file

        // overwrite the file
        FILE *file = fopen(filepath, "w");
        if (file == NULL) {
                switch (errno) {
                        ERRORS_CASE(EACCES, ERR_USERS_ACCES);
                        ERRORS_DEFAULT(ERR_USERS_OPEN);
                }
        }

        for (size_t i = 0; i < storage->ulen; i++) {
                user_t *user = &storage->users[i];
                int err = user_print_line(file, user);
                if (err != 0) {
                        return err;
                }
        }
        fclose(file);
        return 0;
}

int users_update(users_t *storage,
                 const char *username,
                 const pin_hash_t pin_hash) {
        int err = 0;

        user_t *user = NULL;
        for (size_t i = 0; i < storage->ulen; i++) {
                if (strcmp(storage->users[i].username, username) == 0) {
                        user = &storage->users[i];
                        break;
                }
        }
        if (user == NULL) {
                err = users_add(storage, username, pin_hash, false);
        } else {
                memcpy((void*)user->pin_hash, pin_hash, PIN_HASH_LEN);
        }
        if (err != 0) {
                return err;
        }
        return 0;
}

int users_remove(users_t *storage,
                const char *username) {
        int pos = -1;
        for (size_t i = 0; i < storage->ulen; i++) {
                if (strcmp(storage->users[i].username, username) == 0) {
                        pos = i;
                        break;
                }
        }
        if (pos == -1) {
                return ERR_USERS_USER_NOT_FOUND;
        }
        if (storage->users[pos]._allocated) {
                free((void*)storage->users[pos].username);
        }
        for (size_t i = pos; i < storage->ulen - 1; i++) {
                storage->users[i].username = storage->users[i + 1].username;
                memcpy((void*)storage->users[i].pin_hash,
                       storage->users[i + 1].pin_hash,
                       PIN_HASH_LEN);
                storage->users[i]._allocated = storage->users[i + 1]._allocated;
        }
        memset(&storage->users[storage->ulen - 1], 0, sizeof(user_t));
        storage->ulen--;
        return 0;
}

user_t* user_new() {
        user_t *user = malloc(sizeof(user_t));
        if (user == NULL) {
                return NULL;
        }
        user->username = NULL;
        user->_allocated = false;
        return user;
}

void user_free(user_t *user) {
        if (user->_allocated) {
                free((void*)user->username);
        }
        free(user);
}

struct user_iterator {
        const user_t *users;
        size_t len;
        size_t pos;
};

user_iterator_t* users_iterate(users_t *storage) {
        user_iterator_t *iter = malloc(sizeof(user_iterator_t));
        iter->users = storage->users;
        iter->len = storage->ulen;
        iter->pos = 0;
        return iter;
}

bool users_iterator_next(user_iterator_t *iter, user_t *out) {
        if (iter->pos >= iter->len) {
                return false;
        }
        // reuse username allocated field, realloc username if needed, copy pin hash via memcpy
        const user_t *src = &iter->users[iter->pos];
        if (!out->_allocated) {
                out->username = malloc(strlen(src->username) + 1);
                if (out->username == NULL) {
                        return false;
                }
                out->_allocated = true;
        }
        if (strlen(src->username) + 1 > strlen(out->username)) {
                out->username = realloc((void*)out->username, strlen(src->username) + 1);
                if (out->username == NULL) {
                        return false;
                }
        }
        strcpy((char*)out->username, src->username);
        memcpy((void*)out->pin_hash, src->pin_hash, PIN_HASH_LEN);
        iter->pos++;
        return true;
}

int user_print(FILE *out, user_t *user, user_print_fmt format) {
        int err = 0;
        if (format & USER_PRINT_USERNAME) {
                err = fprintf(out, "%s", user->username);
                if (err < 0) {
                        return ERR_USERS_WRITE;
                }
        }
        if (format & USER_PRINT_PINHASH) {
                err = fprintf(out, ":");
                if (err < 0) {
                        return ERR_USERS_WRITE;
                }
                for (size_t i = 0; i < PIN_HASH_LEN; i++) {
                        err = fprintf(out, "%02x", user->pin_hash[i]);
                        if (err < 0) {
                                return ERR_USERS_WRITE;
                        }
                }
        }
        return 0;
}

const char* user_get_name(user_t *user) {
        size_t namelen = strlen(user->username);
        char *name = malloc(namelen + 1);
        if (name == NULL) {
                return NULL;
        }
        strcpy(name, user->username);
        return name;
}

bool user_check_pin(user_t *user, pin_hash_t pin_hash) {
        return memcmp(user->pin_hash, pin_hash, PIN_HASH_LEN) == 0;
}

void users_list_free(user_t *users, const size_t len) {
        for (size_t i = 0; i < len; i++) {
                if (!users[i]._allocated) {
                        continue;
                }
                free((void*)(users[i].username));
        }
        free(users);
}

void users_free(users_t *storage) {
        if (storage->users != NULL) {
                for (size_t i = 0; i < storage->ulen; i++) {
                        if (!storage->users[i]._allocated) {
                                continue;
                        }
                        free((void*)(storage->users[i].username));
                }
                free(storage->users);
                storage->users = NULL;
        }
        storage->ulen = 0;
        storage->ucap = 0;
        free(storage);
}

static int users_add(users_t *storage,
                const char *username,
                const pin_hash_t pin,
                const bool allocated) {
        int err = 0;

        err = users_resize(storage);
        if (err != 0) {
                return err;
        }

        user_t *user = &storage->users[storage->ulen];
        user->username = username;
        memcpy((void*)user->pin_hash, pin, PIN_HASH_LEN);
        user->_allocated = allocated;
        storage->ulen++;
        return 0;
}

static int users_resize(users_t *storage) {
        if (storage->ulen + 1 <= storage->ucap) {
                return 0;
        }
        const size_t newcap = storage->ucap == 0 ? 1 : storage->ucap * 2;
        user_t *new_users = realloc(storage->users, newcap * sizeof(user_t));
        if (new_users == NULL) {
                return -1;
        }
        storage->users = new_users;
        storage->ucap = newcap;
        return 0;
}

static int users_scan_line(FILE *file, char **username, pin_hash_t pin_hash) {
        // line format
        // <username:string>:<pin_hash:binary>\n
        int err = 0;

        char *line = NULL;
        size_t len = 0;
        ssize_t read = getline(&line, &len, file);
        if (read == -1) {
                if (feof(file)) {
                        err = ERR_READ_EOF;
                } else {
                        err = ERR_USERS_READ;
                }
                goto USERS_SCAN_LINE_ERR;
        }
        char *colon = strchr(line, ':');
        if (colon == NULL) {
                err = ERR_USERS_INVALID_FORMAT;
                goto USERS_SCAN_LINE_ERR;
        }
        *colon = '\0';
        *username = strdup(line);
        if (*username == NULL) {
                err = -1;
                goto USERS_SCAN_LINE_ERR;
        }
        memcpy(pin_hash, colon + 1, PIN_HASH_LEN);

USERS_SCAN_LINE_ERR:
        free(line);
        if (err != 0 && *username != NULL) {
                free(*username);
        }
        return err;
}

static int user_print_line(FILE *file, const user_t *user) {
        fprintf(file, "%s:", user->username);
        fwrite(user->pin_hash, 1, PIN_HASH_LEN, file);
        fprintf(file, "\n");
        return 0;
}

static int user_copy(user_t *dst, const user_t *src) {
        const size_t srclen = strlen(src->username);
        if (dst->_allocated) {
                const size_t dstlen = dst->username != NULL ?
                        strlen(dst->username) + 1 : 0;
                if (srclen > dstlen) {
                        dst->username = realloc((void*)dst->username, srclen);
                        if (dst->username == NULL) {
                                return -1;
                        }
                }
                memcpy((void*)dst->username, src->username, srclen);
        } else {
                dst->username = strdup(src->username);
                if (dst->username == NULL) {
                        return -1;
                }
                dst->_allocated = true;
        }
        memcpy((void*)dst->pin_hash, src->pin_hash, PIN_HASH_LEN);
        return 0;
}

