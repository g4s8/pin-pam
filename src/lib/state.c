/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#include "utils.h"

#include "state.h"

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

struct entry {
        char *user;
        uint8_t attempts; // number of invalid pin enter attempts
};

typedef struct entry entry_t;

struct state {
        entry_t *entries;
        uint8_t len;
        uint8_t cap;

        bool modified;
};



static int scan_entry_line(FILE *f, entry_t *entry);

#define ERR_STATE_READ_EOF -101

state_t* state_new() {
        state_t *state = malloc(sizeof(state_t));
        if (state == NULL) {
                return NULL;
        }
        state->entries = NULL;
        state->len = 0;
        state->cap = 0;
        return state;
}

int state_load(state_t *state, const char *path) {
        FILE *f = fopen(path, "r");
        if (f == NULL) {
                switch (errno) {
                        case ENOENT:
                                state->cap = 0;
                                state->len = 0;
                                return 0; // file not found, no error
                        ERRORS_CASE(EACCES, ERR_STATE_FILE_ACCESS);
                        ERRORS_DEFAULT(ERR_STATE_OPEN);
                }
        }

        int err = 0;
        while (!feof(f)) {
                entry_t entry;
                memset(&entry, 0, sizeof(entry_t));
                err = scan_entry_line(f, &entry);
                if (err != 0) {
                        break;
                }
                if (state->cap == state->len) {
                        state->cap = state->cap == 0 ? 16 : state->cap * 2;
                        state->entries = realloc(state->entries, state->cap * sizeof(entry_t));
                        if (state->entries == NULL) {
                                free(entry.user);
                                err = -1;
                                break;
                        }
                }
                state->entries[state->len++] = entry;
        }
        if (err == ERR_STATE_READ_EOF) {
                err = 0;
        }

        if (fclose(f) != 0) {
                free(state->entries);
        }
        return err;
}

int state_save(state_t *state, const char *path) {
        if (!state->modified) {
                return 0;
        }

        // create file if not exist
        FILE *f = fopen(path, "wb+");
        if (f == NULL) {
                switch (errno) {
                        ERRORS_CASE(ENOENT, ERR_STATE_FILE_NOT_FOUND);
                        ERRORS_CASE(EACCES, ERR_STATE_FILE_ACCESS);
                        ERRORS_DEFAULT(ERR_STATE_OPEN);
                }
        }

        int err = 0;
        for (uint8_t i = 0; i < state->len; i++) {
                entry_t *entry = &state->entries[i];
                if (fprintf(f, "%s:%d\n", entry->user, entry->attempts) < 0) {
                        err = ERR_STATE_WRITE;
                        break;
                }
        }

        if (fclose(f) != 0) {
                err = ERR_STATE_WRITE;
        }
        return err;
}

void state_free(state_t *state) {
        for (int i = 0; i < state->len; i++) {
                free(state->entries[i].user);
        }
        free(state->entries);
}

void state_get_attempts(state_t *state, const char *user, uint8_t *attempts) {
        for (uint8_t i = 0; i < state->len; i++) {
                entry_t *entry = &state->entries[i];
                if (strcmp(entry->user, user) == 0) {
                        *attempts = entry->attempts;
                        return;
                }
        }
        *attempts = 0;
}

void state_set_attempts(state_t *state, const char *user, uint8_t attempts) {
        state->modified = true;

        for (uint8_t i = 0; i < state->len; i++) {
                entry_t *entry = &state->entries[i];
                if (strcmp(entry->user, user) == 0) {
                        entry->attempts = attempts;
                        return;
                }
        }
        if (state->cap == state->len) {
                state->cap = state->cap == 0 ? 16 : state->cap * 2;
                state->entries = realloc(state->entries, state->cap * sizeof(entry_t));
        }
        entry_t entry;
        entry.user = strdup(user);
        entry.attempts = attempts;
        state->entries[state->len++] = entry;
}

static int scan_entry_line(FILE *f, entry_t *entry) {
        int err = 0;

        char *line = NULL;
        size_t len = 0;
        ssize_t r = getline(&line, &len, f);
        if (r == -1) {
                if (feof(f)) {
                        err = ERR_STATE_READ_EOF;
                } else {
                        err = ERR_STATE_READ;
                }
                goto SCAN_ENTRY_LINE_RET;
        }

        char *colon = strchr(line, ':');
        if (colon == NULL) {
                err = ERR_STATE_INVALID_FILE;
                goto SCAN_ENTRY_LINE_RET;
        }
        *colon = '\0';
        entry->user = strdup(line);
        if (entry->user == NULL) {
                err = ERR_STATE_READ;
                goto SCAN_ENTRY_LINE_RET;
        }
        // get the rest of line and convert it to uint8_t
        entry->attempts = (uint8_t)strtol(colon + 1, NULL, 10);
        if (entry->attempts == 0 && errno == EINVAL) {
                err = ERR_STATE_INVALID_FILE;
                goto SCAN_ENTRY_LINE_RET;
        }

SCAN_ENTRY_LINE_RET:
        if (err != 0 && entry->user != NULL) {
                free(entry->user);
        }
        free(line);
        return err;
}
