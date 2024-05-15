/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "raise_timeout_coroutine.h"
#include "coroutine_stack.h"

struct ColodRaiseCoroutine {
    Coroutine coroutine;
    ColodRaiseCoroutine **ptr;
    ColodQmpState *qmp;
    const ColodContext *ctx;
};

static gboolean _colod_raise_timeout_co(Coroutine *coroutine,
                                        ColodRaiseCoroutine *this) {
    int ret;

    co_begin(gboolean, G_SOURCE_CONTINUE);

    co_recurse(ret = qmp_wait_event_co(coroutine, this->qmp, 0,
                                       "{'event': 'STOP'}", NULL));
    if (ret < 0) {
        return G_SOURCE_REMOVE;
    }

    co_recurse(ret = qmp_wait_event_co(coroutine, this->qmp, 0,
                                       "{'event': 'RESUME'}", NULL));
    if (ret < 0) {
        return G_SOURCE_REMOVE;
    }

    co_end;

    return G_SOURCE_REMOVE;
}

static gboolean colod_raise_timeout_co(gpointer data) {
    ColodRaiseCoroutine *this = data;
    Coroutine *coroutine = &this->coroutine;
    gboolean ret;

    co_enter(coroutine, ret = _colod_raise_timeout_co(coroutine, this));
    if (coroutine->yield) {
        return GPOINTER_TO_INT(coroutine->yield_value);
    }

    qmp_set_timeout(this->qmp, this->ctx->qmp_timeout_low);

    colod_assert_remove_one_source(coroutine);
    *this->ptr = NULL;
    g_free(this);
    return ret;
}

static gboolean colod_raise_timeout_co_wrap(
        G_GNUC_UNUSED GIOChannel *channel,
        G_GNUC_UNUSED GIOCondition revents,
        gpointer data) {
    return colod_raise_timeout_co(data);
}

void colod_raise_timeout_coroutine_free(ColodRaiseCoroutine **ptr) {
    if (!*ptr) {
        return;
    }

    g_idle_add(colod_raise_timeout_co, *ptr);

    while (*ptr) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }
}

void colod_raise_timeout_coroutine(ColodRaiseCoroutine **ptr,
                                   ColodQmpState *qmp,
                                   const ColodContext *ctx) {
    ColodRaiseCoroutine *this;
    Coroutine *coroutine;

    if (*ptr) {
        return;
    }

    qmp_set_timeout(qmp, ctx->qmp_timeout_high);

    this = g_new0(ColodRaiseCoroutine, 1);
    coroutine = &this->coroutine;
    coroutine->cb.plain = colod_raise_timeout_co;
    coroutine->cb.iofunc = colod_raise_timeout_co_wrap;
    this->qmp = qmp;
    this->ctx = ctx;
    this->ptr = ptr;
    *ptr = this;

    g_idle_add(colod_raise_timeout_co, this);
}
