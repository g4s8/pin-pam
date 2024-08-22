/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#include "./lib/users.h"
#include "./lib/types.h"
#include "./lib/crypt.h"
#include "./lib/state.h"
#include "./config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


static void usage(const char *name) __attribute__((noreturn));

static void panic(const char *msg, const char *err) __attribute__((noreturn));
static void checkerr(int err, const char *msg);
static void checkerr_hash(int err, const char *msg);
static void checkerr_state(int err, const char *msg);

typedef enum {
        ACTION_NONE = 0,
        ACTION_LIST,
        ACTION_ADD,
        ACTION_REMOVE,
        ACTIONS_CHECK,
        ACTION_RESET,
        ACTION_HELP,
        ACTION_VERSION,
} action_t;

typedef struct cli_args {
        const char *cmd;
        action_t action;
        union {
                struct {
                        char *user;
                        bool update;
                        pin_source_t pin;
                } add;
                struct {
                        char *user;
                } remove;
                struct {
                        char *user;
                        pin_source_t pin;
                } check;
                struct {
                        char *user;
                } reset;
        };
} cli_args_t;

static int read_pin(pin_source_t pin);

static void parse_args(cli_args_t *args, int argc, char **argv) {
        if (argc < 2) {
                fprintf(stderr, "Error: command not specified\n");
                usage(argv[0]);
        }

        args->cmd = argv[0];

        int i = 1;
        while (i < argc) {
                if (strcmp(argv[i], "--help") == 0) {
                        args->action = ACTION_HELP;
                        break;
                } else if (strcmp(argv[i], "--version") == 0) {
                        args->action = ACTION_VERSION;
                        break;
                } else if (strcmp(argv[i], "list") == 0) {
                        args->action = ACTION_LIST;
                        break;
                } else if (strcmp(argv[i], "add") == 0) {
                        args->action = ACTION_ADD;
                        i++;
                        if (i >= argc) {
                                fprintf(stderr, "Error: user not specified\n");
                                usage(argv[0]);
                        }
                        if (strcmp(argv[i], "--update") == 0) {
                                args->add.update = true;
                                i++;
                                if (i >= argc) {
                                        fprintf(stderr, "Error: user not specified\n");
                                        usage(argv[0]);
                                }
                        }
                        args->add.user = argv[i];
                        int err = read_pin(args->add.pin);
                        if (err != 0) {
                                fprintf(stderr, "Invalid PIN, err=%d\n", err);
                                usage(argv[0]);
                        }
                        break;
                } else if (strcmp(argv[i], "remove") == 0) {
                        args->action = ACTION_REMOVE;
                        i++;
                        if (i >= argc) {
                                fprintf(stderr, "Error: user not specified\n");
                                usage(argv[0]);
                        }
                        args->remove.user = argv[i];
                        break;
                } else if (strcmp(argv[i], "check") == 0) {
                        args->action = ACTIONS_CHECK;
                        i++;
                        if (i >= argc) {
                                fprintf(stderr, "Error: user not specified\n");
                                usage(argv[0]);
                        }
                        args->check.user = argv[i];
                        int err = read_pin(args->check.pin);
                        if (err != 0) {
                                fprintf(stderr, "Error: invalid PIN, err=%d\n", err);
                                usage(argv[0]);
                        }
                        break;
                } else if (strcmp(argv[i], "reset") == 0) {
                        args->action = ACTION_RESET;
                        i++;
                        args->reset.user = argv[i];
                        break;
                } else {
                        fprintf(stderr, "Error: unknown command: %s\n", argv[i]);
                        usage(argv[0]);
                }
        }

        // validate
        switch (args->action) {
                case ACTION_NONE:
                        fprintf(stderr, "Error: action not specified\n");
                        usage(argv[0]);
                        break;
                case ACTION_ADD:
                        if (args->add.user == NULL) {
                                fprintf(stderr, "Error: user or pin not specified\n");
                                usage(argv[0]);
                        }
                        break;
                case ACTION_REMOVE:
                        if (args->remove.user == NULL) {
                                fprintf(stderr, "Error: user not specified\n");
                                usage(argv[0]);
                        }
                        break;
                case ACTIONS_CHECK:
                        if (args->check.user == NULL) {
                                fprintf(stderr, "Error: user or pin not specified\n");
                                usage(argv[0]);
                        }
                        break;
                case ACTION_RESET:
                        if (args->reset.user == NULL) {
                                fprintf(stderr, "Error: user not specified\n");
                                usage(argv[0]);
                        }
                        break;
                default:
                        break;
        }
}

// actions state machine
typedef void (*action_fn_t)(cli_args_t *args, users_t *storage, bool *modified);

static void action_list(cli_args_t *args, users_t *storage, bool *modified);
static void action_add(cli_args_t *args, users_t *storage, bool *modified);
static void action_remove(cli_args_t *args, users_t *storage, bool *modified);
static void action_check(cli_args_t *args, users_t *storage, bool *modified);
static void action_reset(cli_args_t *args, users_t *storage, bool *modified);
static void action_help(cli_args_t *args, users_t *storage, bool *modified);
static void action_version(cli_args_t *args, users_t *storage, bool *modified);

static action_fn_t actions[] = {
        [ACTION_LIST] = action_list,
        [ACTION_ADD] = action_add,
        [ACTION_REMOVE] = action_remove,
        [ACTIONS_CHECK] = action_check,
        [ACTION_RESET] = action_reset,
        [ACTION_HELP] = action_help,
        [ACTION_VERSION] = action_version,
};

/*
 * The program to edit the users file.
 * Usage:
 *   fauth-edit list - print users
 *   fauth-edit add --update <user> - add or update user, read pin from stdin
 *   fauth-edit remove <user> - remove user
 *   fauth-edit check <user> - check user pin, read pin from stdin
 *   fauth-edit --help - print help
 *   fauth-edit --version - print version
 */
int main(int argc, char** argv) {
        cli_args_t args = {0};
        parse_args(&args, argc, argv);

        int err = 0;
        users_t *storage = users_new(10);
        bool load_storage = false;
        switch (args.action) {
                case ACTION_LIST:
                case ACTION_ADD:
                case ACTION_REMOVE:
                case ACTIONS_CHECK:
                        load_storage = true;
                        break;
                default:
                        load_storage = false;
                        break;
        }
        if (load_storage) {
                err = users_load(storage, srcfile);
                checkerr(err, "Open users file");
        }

        bool modified = false;
        actions[args.action](&args, storage, &modified);

        if (modified) {
                err = users_dump(storage, srcfile);
                checkerr(err, "Dump users file");
        }

        users_free(storage);
        return 0;
}

static void panic(const char *msg, const char *err) {
        fprintf(stderr, "Panic: %s: %s\n", msg, err);
        exit(1);
}

static void checkerr_hash(int err, const char *msg) {
        switch (err) {
                case HASH_ERR_CTX:
                        panic(msg, "Could not create hash context");
                case HASH_ERR_DIGEST_INIT:
                        panic(msg, "Could not initialize digest");
                case HASH_ERR_DIGEST_UPDATE:
                        panic(msg, "Could not update digest");
                case HASH_ERR_DIGEST_FINAL:
                        panic(msg, "Could not finalize digest");
                default:
                        return;
        }
}

static void checkerr(int err, const char *msg) {
        switch (err) {
                case ERR_USERS_OPEN:
                        panic(msg, "Could not open file");
                case ERR_USERS_ACCES:
                        panic(msg, "Could not access file");
                case ERR_USERS_NOT_FOUND:
                        panic(msg, "File not found");
                case ERR_USERS_READ:
                        panic(msg, "Could not read file");
                case ERR_USERS_WRITE:
                        panic(msg, "Could not write file");
                case ERR_USERS_CLOSE:
                        panic(msg, "Could not close file");
                case ERR_USERS_INVALID_FORMAT:
                        panic(msg, "Invalid file format");
                case ERR_USERS_USER_NOT_FOUND:
                        panic(msg, "User not found");
                default:
                        return;
        }
}
static void checkerr_state(int err, const char *msg) {
        switch (err) {
                case ERR_STATE_OPEN:
                        panic(msg, "Could not open file");
                case ERR_STATE_READ:
                        panic(msg, "Could not read file");
                case ERR_STATE_INVALID_FILE:
                        panic(msg, "Invalid file format");
                case ERR_STATE_FILE_NOT_FOUND:
                        panic(msg, "File not found");
                case ERR_STATE_FILE_ACCESS:
                        panic(msg, "Could not access file");
                case ERR_STATE_WRITE:
                        panic(msg, "Could not write file");
                default:
                        return;
        }
}

static void usage(const char *name) {
        fprintf(stderr, "Usage: %s list\n", name);
        fprintf(stderr, "       %s add --update <user>\n", name);
        fprintf(stderr, "       %s remove <user>\n", name);
        fprintf(stderr, "       %s check <user>\n", name);
        fprintf(stderr, "       %s --reset <user>\n", name);
        fprintf(stderr, "       %s --help\n", name);
        fprintf(stderr, "       %s --version\n", name);
        exit(1);
}

static void action_list(cli_args_t *args, users_t *storage, bool *modified) {
        printf("Users:\n");
        user_t *user = user_new();
        user_iterator_t *iter = users_iterate(storage);
        while (users_iterator_next(iter, user)) {
                printf(" * ");
                user_print(stdout, user,
                           USER_PRINT_USERNAME | USER_PRINT_PINHASH);
                printf("\n");
        }
        user_free(user);
}

static void action_add(cli_args_t *args, users_t *storage, bool *modified) {
        int err = 0;
        pin_hash_t pin_hash;
        err = hash_pin(args->add.pin, pin_hash);
        checkerr_hash(err, "Hash pin");

        err = users_update(storage, args->add.user, pin_hash);
        checkerr(err, "Add user");
        *modified = true;
        printf("User %s added\n", args->add.user);
}

static void action_remove(cli_args_t *args, users_t *storage, bool *modified) {
        int err = users_remove(storage, args->remove.user);
        checkerr(err, "Remove user");
        *modified = true;
        printf("User %s removed\n", args->remove.user);
}

static void action_check(cli_args_t *args, users_t *storage, bool *modified) {
        user_t *user = user_new();
        int err = users_find(storage, args->check.user, user);
        checkerr(err, "Get user");

        pin_hash_t pin_hash;
        err = hash_pin(args->check.pin, pin_hash);
        checkerr_hash(err, "Hash pin");

        bool valid = user_check_pin(user, pin_hash);
        if (!valid) {
                fprintf(stderr, "Invalid pin\n");
        } else {
                printf("Valid pin\n");
        }
        user_free(user);
        if (!valid) {
                exit(1);
        }

        state_t *state = state_new();
        err = state_load(state, varfile);
        checkerr_state(err, "Load state file");
        uint8_t attempts;
        state_get_attempts(state, args->check.user, &attempts);
        fprintf(stderr, "Attempts: %d", attempts);
        if (attempts >= 3) {
                fprintf(stderr, " (user %s is locked)", args->check.user);
        }
        fprintf(stderr, "\n");
        state_free(state);
}

static void action_reset(cli_args_t *args, users_t *storage, bool *modified) {
        int err = 0;
        state_t *state = state_new();
        err = state_load(state, varfile);
        checkerr_state(err, "Load state file");
        state_set_attempts(state, args->reset.user, 0);
        err = state_save(state, varfile);
        checkerr_state(err, "Save state file");
        state_free(state);
        printf("User %s hase been reset\n", args->reset.user);
}

static void action_help(cli_args_t *args, users_t *storage, bool *modified) {
        fprintf(stderr, "Help: %s\n", args->cmd);
        usage(args->cmd);
}

static void action_version(cli_args_t *args, users_t *storage, bool *modified) {
        printf("Build version: %s\n", BUILD_VERSION);
}

static int read_pin(pin_source_t pin) {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        printf("Enter pin: ");
        fflush(stdout);
        int err = 0;
        // read exactly 4 digits, if EOF or newline, return error -1
        for (size_t i = 0; i < PIN_SOURCE_LEN; i++) {
                int c = getchar();
                if (c == EOF || c == '\n') {
                        err = -1;
                        goto READ_PIN_RET;
                }
                if (c < '0' || c > '9') {
                        err = -2;
                        goto READ_PIN_RET;
                }
                pin[i] = c - '0';
                putchar('*');
        }

READ_PIN_RET:
        printf("\n");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return err;
}
