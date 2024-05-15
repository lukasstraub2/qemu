/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>

#include "delay_coroutine.h"

static gboolean delay_coroutine_cb(gpointer data) {
    ColodCallback *this = data;
    DelayCallbackFunc func = (DelayCallbackFunc) this->func;

    func(this->user_data);

    QLIST_REMOVE(this, next);
    g_free(this);
    return G_SOURCE_REMOVE;
}

void delay_coroutine_new(DelayCoroutineHead *head, guint delay,
                         DelayCallbackFunc func, gpointer user_data) {
    ColodCallback *this;

    this = g_new0(ColodCallback, 1);
    this->func = (ColodCallbackFunc) func;
    this->user_data = user_data;

    QLIST_INSERT_HEAD(head, this, next);

    g_timeout_add(delay, delay_coroutine_cb, this);
}

void delay_coroutines_free(DelayCoroutineHead *head) {
    while (!QLIST_EMPTY(head)) {
        ColodCallback *co = QLIST_FIRST(head);
        g_idle_add(delay_coroutine_cb, co);
    }

    while (!QLIST_EMPTY(head)) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }
}
