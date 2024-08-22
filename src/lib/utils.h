/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _UTILS_H
#define _UTILS_H

#include "types.h"

#define ERRORS_CASE(err, code) case err: return code;
#define ERRORS_DEFAULT(code) default: return code;

enum {
        ERR_READ_PIN_SHORT = 1,
        ERR_READ_PIN_BADCHAR,
};

int read_pin(const char *prompt, pin_source_t pin);

#endif
