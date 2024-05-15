/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef DELAY_COROUTINE_H
#define DELAY_COROUTINE_H

#include <glib-2.0/glib.h>

#include "util.h"

typedef void (*DelayCallbackFunc)(gpointer data);
typedef ColodCallbackHead DelayCoroutineHead;

void delay_coroutine_new(DelayCoroutineHead *head, guint delay,
                         DelayCallbackFunc func, gpointer data);
void delay_coroutines_free(DelayCoroutineHead *head);

#endif // DELAY_COROUTINE_H
