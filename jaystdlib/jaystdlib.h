/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef JAYSTDLIB_H
#define JAYSTDLIB_H

#include "jay_internal.h"

/*
 * Dispatch a Jay standard-library function.
 * Returns true when name belongs to the standard library.
 */
bool jaystdlib_call(const char *name, Value *arguments,
                    size_t argument_count, Value *result, int line);

#endif
