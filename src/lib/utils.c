/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#include "types.h"
#include "utils.h"

#include <termios.h>
#include <unistd.h>

int read_pin(const char *prompt, pin_source_t pin) {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        printf("%s: ", prompt);
        fflush(stdout);
        int err = 0;
        // read exactly 4 digits, if EOF or newline, return error -1
        for (size_t i = 0; i < PIN_SOURCE_LEN; i++) {
                int c = getchar();
                if (c == EOF || c == '\n') {
                        err = ERR_READ_PIN_SHORT;
                        goto READ_PIN_RET;
                }
                if (c < '0' || c > '9') {
                        err = ERR_READ_PIN_BADCHAR;
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
