/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>

#include "watchdog.h"
#include "main_coroutine.h"

typedef struct ColodWatchdog {
    Coroutine coroutine;
    const ColodContext *ctx;
    guint interval;
    guint timer_id;
    gboolean quit;
} ColodWatchdog;

void colod_watchdog_refresh(ColodWatchdog *state) {
    if (state->timer_id) {
        g_source_remove(state->timer_id);
        state->timer_id = g_timeout_add_full(G_PRIORITY_LOW,
                                             state->interval,
                                             state->coroutine.cb.plain,
                                             &state->coroutine, NULL);
    }
}

static void colod_watchdog_event_cb(gpointer data,
                                    G_GNUC_UNUSED ColodQmpResult *result) {
    ColodWatchdog *state = data;
    colod_watchdog_refresh(state);
}

static gboolean _colod_watchdog_co(Coroutine *coroutine);
static gboolean colod_watchdog_co(gpointer data) {
    ColodWatchdog *state = data;
    Coroutine *coroutine = &state->coroutine;
    gboolean ret;

    co_enter(coroutine, ret = _colod_watchdog_co(coroutine));
    if (coroutine->yield) {
        return GPOINTER_TO_INT(coroutine->yield_value);
    }

    colod_assert_remove_one_source(coroutine);
    return ret;
}

static gboolean colod_watchdog_co_wrap(
        G_GNUC_UNUSED GIOChannel *channel,
        G_GNUC_UNUSED GIOCondition revents,
        gpointer data) {
    return colod_watchdog_co(data);
}

static gboolean _colod_watchdog_co(Coroutine *coroutine) {
    ColodWatchdog *state = (ColodWatchdog *) coroutine;
    int ret;
    GError *local_errp = NULL;

    co_begin(gboolean, G_SOURCE_CONTINUE);

    while (!state->quit) {
        state->timer_id = g_timeout_add_full(G_PRIORITY_LOW,
                                             state->interval,
                                             coroutine->cb.plain,
                                             coroutine, NULL);
        co_yield_int(G_SOURCE_REMOVE);
        if (state->quit) {
            break;
        }
        state->timer_id = 0;

        co_recurse(ret = colod_check_health_co(coroutine, state->ctx->main_coroutine,
                                               &local_errp));
        if (ret < 0) {
            log_error_fmt("colod check health: %s", local_errp->message);
            g_error_free(local_errp);
            local_errp = NULL;
            return G_SOURCE_REMOVE;
        }
    }

    co_end;

    return G_SOURCE_REMOVE;
}

void colo_watchdog_free(ColodWatchdog *state) {

    if (!state->interval) {
        g_free(state);
        return;
    }

    state->quit = TRUE;

    qmp_del_notify_event(state->ctx->qmp, colod_watchdog_event_cb, state);

    if (state->timer_id) {
        g_source_remove(state->timer_id);
        state->timer_id = 0;
        g_idle_add(colod_watchdog_co, &state->coroutine);
    }

    while (!state->coroutine.quit) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }

    g_free(state);
}

ColodWatchdog *colod_watchdog_new(const ColodContext *ctx) {
    ColodWatchdog *state;
    Coroutine *coroutine;

    state = g_new0(ColodWatchdog, 1);
    coroutine = &state->coroutine;
    coroutine->cb.plain = colod_watchdog_co;
    coroutine->cb.iofunc = colod_watchdog_co_wrap;
    state->ctx = ctx;
    state->interval = ctx->watchdog_interval;

    if (state->interval) {
        g_idle_add(colod_watchdog_co, coroutine);
        qmp_add_notify_event(ctx->qmp, colod_watchdog_event_cb, state);
    }
    return state;
}
