/*
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include <config.h>

#ifndef ETC_USERS_PATH
#define ETC_USERS_PATH "/tmp/etc-pinpam-users"
#endif

#ifndef VAR_USERS_PATH
#define VAR_USERS_PATH "/tmp/var-pinmap-users"
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "local"
#endif

static const char * const srcfile = ETC_USERS_PATH;
static const char * const varfile = VAR_USERS_PATH;

#endif
