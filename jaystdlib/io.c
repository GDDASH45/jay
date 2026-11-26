/* SPDX-License-Identifier: GPL-2.0-only */
#include "jaystdlib.h"

static bool argument_count_is(const char *name, size_t got,
                              size_t expected, int line)
{
    if (got == expected)
        return true;

    char message[192];
    snprintf(message, sizeof(message),
             "%s() expects %zu argument%s, got %zu",
             name, expected, expected == 1 ? "" : "s", got);
    fail_at(line, message);
    return false;
}

static bool require_type(const char *name, Value value,
                         ValueType expected, int line)
{
    if (value.type == expected)
        return true;

    char message[192];
    snprintf(message, sizeof(message),
             "%s() expects %s, got %s",
             name, type_name(expected), type_name(value.type));
    fail_at(line, message);
    return false;
}

bool jaystdlib_call(const char *name, Value *arguments,
                    size_t argument_count, Value *result, int line)
{
    *result = make_void();

    if (strcmp(name, "print") == 0) {
        for (size_t i = 0; i < argument_count; i++)
            print_value(arguments[i]);

        putchar('\n');
        return true;
    }

    if (strcmp(name, "putchar") == 0) {
        if (!argument_count_is(name, argument_count, 1, line))
            return true;

        int character;
        if (arguments[0].type == V_CHAR) {
            character = (unsigned char)arguments[0].as.c;
        } else if (arguments[0].type == V_INT) {
            character = (unsigned char)arguments[0].as.i;
        } else {
            fail_at(line, "putchar() expects char or int");
            return true;
        }

        if (putchar(character) == EOF) {
            fail_at(line, "putchar() failed to write to stdout");
            return true;
        }

        *result = make_int(character);
        return true;
    }

    if (strcmp(name, "puts") == 0) {
        if (!argument_count_is(name, argument_count, 1, line))
            return true;

        if (!require_type(name, arguments[0], V_STR, line))
            return true;

        if (puts(arguments[0].as.s) == EOF) {
            fail_at(line, "puts() failed to write to stdout");
            return true;
        }

        *result = make_int(0);
        return true;
    }

    if (strcmp(name, "putnum") == 0) {
        if (!argument_count_is(name, argument_count, 1, line))
            return true;

        if (!require_type(name, arguments[0], V_INT, line))
            return true;

        if (printf("%lld\n", arguments[0].as.i) < 0)
            fail_at(line, "putnum() failed to write to stdout");

        return true;
    }

    return false;
}
