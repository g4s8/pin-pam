/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#include "../lib/types.h"
#include "../lib/users.h"
#include "../lib/state.h"
#include "../lib/crypt.h"
#include "../lib/utils.h"
#include "../config.h"

#include <security/_pam_types.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define pamerr(h, m, e) do { \
        pam_syslog(h, LOG_ERR, "%s: %s", m, e); \
        pam_error(h, "%s", m); \
} while (0)

const int storage_capacity = 10;

const int pin_retry_attempts = 3;

static bool checkerr_users(pam_handle_t *pamh, int err, const char *msg);
static bool checkerr_hash(pam_handle_t *pamh, int err, const char *msg);
static bool checkerr_state(pam_handle_t *pamh, int err, const char *msg);

static int read_pin_pam(pam_handle_t *pamh, const char *prompt, pin_source_t out);

/* Define the entry point for the 'authenticate' function */
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
        const char *username;
        int pam_code;

        // get the username entered by the user
        pam_code = pam_get_user(pamh, &username, NULL);

        if (pam_code != PAM_SUCCESS) {
                pam_syslog(pamh, LOG_ERR, "Failed to get pam user");
                return pam_code;
        }
        pam_syslog(pamh, LOG_INFO, "User '%s' is trying to authenticate", username);
       
        int err = 0;
        users_t *storage = users_new(storage_capacity);
        pam_syslog(pamh, LOG_INFO, "Loading users file %s", srcfile);
        err = users_load(storage, srcfile);
        if (!checkerr_users(pamh, err, "Failed to load users file")) {
                users_free(storage);
                return PAM_AUTH_ERR;
        }
        user_t *user = user_new();
        pam_syslog(pamh, LOG_INFO, "Searching for user %s", username);
        err = users_find(storage, username, user);
        if (!checkerr_users(pamh, err, "User not found")) {
                users_free(storage);
                user_free(user);
                return PAM_AUTH_ERR;
        }

        state_t *state = state_new();
        pam_syslog(pamh, LOG_INFO, "Loading state file %s", varfile);
        err = state_load(state, varfile);
        if (!checkerr_state(pamh, err, "Failed to load state file")) {
                users_free(storage);
                user_free(user);
                state_free(state);
                return PAM_AUTH_ERR;
        }
        uint8_t attempts;
        state_get_attempts(state, username, &attempts);
        if (attempts >= pin_retry_attempts) {
                pam_syslog(pamh, LOG_INFO, "Too many attempts. Skip PIN auth");
                users_free(storage);
                user_free(user);
                state_free(state);
                return PAM_AUTH_ERR;
        }

        bool pin_valid = false;
        while (attempts < pin_retry_attempts) {
                pin_source_t pinsrc;
                pam_syslog(pamh, LOG_INFO, "Reading PIN for user %s", username);
                err = read_pin_pam(pamh, "Enter PIN", pinsrc);
                if (err != 0) {
                        memset(pinsrc, 0, PIN_SOURCE_LEN);
                        attempts++;
                        pam_syslog(pamh, LOG_INFO, "Invalid PIN; too short");
                        pam_error(pamh, "Invalid PIN; Retry (%d/%d)",
                                        attempts, pin_retry_attempts);
                        state_set_attempts(state, username, attempts);
                        continue;
                }
                pam_syslog(pamh, LOG_INFO, "PIN read successfully");

                pin_hash_t pinhash;
                err = hash_pin(pinsrc, pinhash);
                // fill the pinsrc with garbage
                memset(pinsrc, 0, PIN_SOURCE_LEN);
                if (!checkerr_hash(pamh, err, "Unknown error, check system logs")) {
                        // return PAM_AUTH_ERR;
                        attempts++;
                        pam_syslog(pamh, LOG_INFO, "Invalid PIN");
                        pam_error(pamh, "Invalid PIN; Retry (%d/%d)",
                                        attempts, pin_retry_attempts);
                        state_set_attempts(state, username, attempts);
                        continue;
                }


                bool valid = user_check_pin(user, pinhash);
                if (valid) {
                        pin_valid = true;
                        pam_syslog(pamh, LOG_INFO, "PIN verified successfully");
                        state_set_attempts(state, username, 0);
                        break;
                } else {
                        attempts++;
                        pam_syslog(pamh, LOG_INFO, "Invalid PIN");
                        pam_error(pamh, "Invalid PIN; Retry (%d/%d)",
                                        attempts, pin_retry_attempts);
                        state_set_attempts(state, username, attempts);
                        continue;
                }
        }

        err = state_save(state, varfile);
        checkerr_state(pamh, err, "Failed to save state file");
        state_free(state);
        user_free(user);
        users_free(storage);
        if (pin_valid) {
                return PAM_SUCCESS;
        } else {
                return PAM_AUTH_ERR;
        }
}

/* Define the entry point for the 'setcred' function */
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    return PAM_SUCCESS;
}

static int read_pin_pam(pam_handle_t *pamh, const char *prompt, pin_source_t out) {
        int ret = 0;
        int pam_code;
        char *pin_response = NULL;
        pam_code = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &pin_response, "%s: ", prompt);
        if (pam_code != PAM_SUCCESS) {
                pam_syslog(pamh, LOG_ERR, "Invalid PIN");
                ret = -1;
        }
        if (strlen(pin_response) != PIN_SOURCE_LEN) {
                pam_syslog(pamh, LOG_ERR, "PIN is too short");
                ret = -1;
                goto READ_PIN_PAN_RET;
        }
        for (size_t i = 0; i < PIN_SOURCE_LEN; i++) {
                if (pin_response[i] < '0' || pin_response[i] > '9') {
                        pam_syslog(pamh, LOG_ERR, "PIN contains invalid characters");
                        ret = -1;
                        goto READ_PIN_PAN_RET;
                }
                out[i] = pin_response[i] - '0';
        }
READ_PIN_PAN_RET:
        // if pin_response is not null, fill it with garbage
        if (pin_response != NULL) {
                memset(pin_response, 0, PIN_SOURCE_LEN);
        }
        free(pin_response);
        return ret;
}

static bool checkerr_users(pam_handle_t *pamh, int err, const char *msg) {
        if (err == 0) return true;
        switch (err) {
                case ERR_USERS_OPEN:
                        pamerr(pamh, msg, "Could not open file");
                        break;
                case ERR_USERS_ACCES:
                        pamerr(pamh, msg, "Could not access file");
                        break;
                case ERR_USERS_NOT_FOUND:
                        pamerr(pamh, msg, "File not found");
                        break;
                case ERR_USERS_READ:
                        pamerr(pamh, msg, "Could not read file");
                        break;
                case ERR_USERS_WRITE:
                        pamerr(pamh, msg, "Could not write file");
                        break;
                case ERR_USERS_CLOSE:
                        pamerr(pamh, msg, "Could not close file");
                        break;
                case ERR_USERS_INVALID_FORMAT:
                        pamerr(pamh, msg, "Invalid file format");
                case ERR_USERS_USER_NOT_FOUND:
                        pamerr(pamh, msg, "User not found");
                        break;
        }
        return false;
}

static bool checkerr_hash(pam_handle_t *pamh, int err, const char *msg) {
        if (err == 0) return true;
        switch (err) {
                case HASH_ERR_CTX:
                        pamerr(pamh, msg, "Could not create context");
                        break;
                case HASH_ERR_DIGEST_INIT:
                        pamerr(pamh, msg, "Could not initialize digest");
                        break;
                case HASH_ERR_DIGEST_UPDATE:
                        pamerr(pamh, msg, "Could not update digest");
                        break;
                case HASH_ERR_DIGEST_FINAL:
                        pamerr(pamh, msg, "Could not finalize digest");
                        break;
        }
        return false;
}

static bool checkerr_state(pam_handle_t *pamh, int err, const char *msg) {
        if (err == 0) return true;
        switch (err) {
                case ERR_STATE_OPEN:
                        pamerr(pamh, msg, "Could not open file");
                        break;
                case ERR_STATE_READ:
                        pamerr(pamh, msg, "Could not read file");
                        break;
                case ERR_STATE_INVALID_FILE:
                        pamerr(pamh, msg, "Invalid file format");
                        break;
                case ERR_STATE_FILE_NOT_FOUND:
                        pamerr(pamh, msg, "File not found");
                        break;
                case ERR_STATE_FILE_ACCESS:
                        pamerr(pamh, msg, "Could not access file");
                        break;
                case ERR_STATE_WRITE:
                        pamerr(pamh, msg, "Could not write file");
                        break;
        }
        return false;
}

