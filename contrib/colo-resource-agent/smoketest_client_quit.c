/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>

#include "base_types.h"
#include "smoketest.h"
#include "coroutine_stack.h"
#include "smoke_util.h"
#include "main_coroutine.h"

typedef struct ClientQuitConfig {
    gboolean client_crash;
} QuitEarlyConfig;

struct SmokeTestcase {
    Coroutine coroutine;
    SmokeColodContext *sctx;
    const QuitEarlyConfig *config;
    gboolean do_quit, quit;
};

static gboolean logged = FALSE;

void colod_syslog(G_GNUC_UNUSED int pri, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fwrite("\n", 1, 1, stderr);
    va_end(args);

    logged = TRUE;
}

static gboolean _testcase_co(Coroutine *coroutine, SmokeTestcase *this) {
    SmokeColodContext *sctx = this->sctx;

    co_begin(gboolean, G_SOURCE_CONTINUE);

    g_timeout_add(200, coroutine->cb.plain, this);
    co_yield_int(G_SOURCE_REMOVE);

    logged = FALSE;
    if (this->config->client_crash) {
        colod_shutdown_channel(sctx->client_ch);
    } else {
        g_io_channel_shutdown(sctx->client_ch, FALSE, NULL);
    }

    g_timeout_add(10, coroutine->cb.plain, this);
    co_yield_int(G_SOURCE_REMOVE);

    g_assert_false(logged);

    colod_quit(this->sctx->cctx.main_coroutine);

    assert(!this->do_quit);
    while (!this->do_quit) {
        progress_source_add(coroutine->cb.plain, this);
        co_yield_int(G_SOURCE_REMOVE);
    }
    this->quit = TRUE;
    co_end;

    return G_SOURCE_REMOVE;
}

static gboolean testcase_co(gpointer data) {
    SmokeTestcase *this = data;
    Coroutine *coroutine = data;
    gboolean ret;

    co_enter(coroutine, ret = _testcase_co(coroutine, this));
    if (coroutine->yield) {
        return GPOINTER_TO_INT(coroutine->yield_value);
    }

    colod_assert_remove_one_source(coroutine);
    return ret;
}

static gboolean testcase_co_wrap(
        G_GNUC_UNUSED GIOChannel *channel,
        G_GNUC_UNUSED GIOCondition revents,
        gpointer data) {
    return testcase_co(data);
}

static SmokeTestcase *testcase_new(SmokeColodContext *sctx,
                                   const QuitEarlyConfig *config) {
    SmokeTestcase *this;
    Coroutine *coroutine;

    this = g_new0(SmokeTestcase, 1);
    coroutine = &this->coroutine;
    coroutine->cb.plain = testcase_co;
    coroutine->cb.iofunc = testcase_co_wrap;
    this->sctx = sctx;
    this->config = config;

    sctx->cctx.qmp_timeout_low = 10;

    g_idle_add(testcase_co, this);
    return this;
}

static void testcase_free(SmokeTestcase *this) {
    this->do_quit = TRUE;

    while (!this->quit) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }

    g_free(this);
}

static void test_run(gconstpointer opaque) {
    GError *errp = NULL;
    const QuitEarlyConfig *config = opaque;
    SmokeColodContext *sctx;
    SmokeTestcase *testcase;

    sctx = smoke_context_new(&errp);
    g_assert_true(sctx);

    testcase = testcase_new(sctx, config);

    daemon_mainloop(&sctx->cctx);

    testcase_free(testcase);
    smoke_context_free(sctx);
}

int main(int argc, char **argv) {
    smoke_init();

    g_test_init(&argc, &argv, NULL);

    g_test_add_data_func("/client_quit/normal",
                         &(QuitEarlyConfig) {
                             .client_crash = FALSE
                         }, test_run);
    g_test_add_data_func("/client_quit/crash",
                         &(QuitEarlyConfig) {
                             .client_crash = TRUE
                         }, test_run);

    return g_test_run();
}
