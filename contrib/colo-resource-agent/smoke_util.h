/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef SMOKE_UTIL_H
#define SMOKE_UTIL_H

#include <glib-2.0/glib.h>
#include "coroutine_stack.h"

#define ch_write_co(...) \
    co_wrap(_ch_write_co(__VA_ARGS__))
void _ch_write_co(Coroutine *coroutine, GIOChannel *channel,
                  const gchar *buf, guint timeout);

#define ch_readln_co(...) \
    co_wrap(_ch_readln_co(__VA_ARGS__))
void _ch_readln_co(Coroutine *coroutine, GIOChannel *channel,
                   gchar **buf, gsize *len, guint timeout);

#endif // SMOKE_UTIL_H
