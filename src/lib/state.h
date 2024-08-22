/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _STATE_H
#define _STATE_H

#include <stdint.h>

typedef struct state state_t;

enum {
        ERR_STATE_OPEN = 1,
        ERR_STATE_READ,
        ERR_STATE_INVALID_FILE,
        ERR_STATE_FILE_NOT_FOUND,
        ERR_STATE_FILE_ACCESS,
        ERR_STATE_WRITE,
};

state_t* state_new();
int state_load(state_t *state, const char *path);
int state_save(state_t *state, const char *path);
void state_get_attempts(state_t *state, const char *user, uint8_t *attempts);
void state_set_attempts(state_t *state, const char *user, uint8_t attempts);
void state_free(state_t *state);

#endif
