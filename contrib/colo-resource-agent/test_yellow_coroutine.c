/*
 * COLO background yellow coroutine test
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "coroutine_stack.h"
#include "yellow_coroutine.h"
#include "util.h"
#include "daemon.h"
#include "netlink.h"

FILE *trace = NULL;
gboolean do_syslog = FALSE;

void colod_trace(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fflush(stderr);

    va_end(args);
}

void colod_syslog(G_GNUC_UNUSED int pri, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fwrite("\n", 1, 1, stderr);
    va_end(args);
}

typedef struct TestCoroutine TestCoroutine;

struct TestCoroutine {
    Coroutine coroutine;
    Cpg *cpg;
    YellowCoroutine *yellow_co;
    ColodContext ctx;
    GMainLoop *mainloop;
};

int _test_co(Coroutine *coroutine, TestCoroutine *this, ColodEvent event) {
    struct {
        guint source_id;
    } *co;

    co_frame(co, sizeof(*co));
    co_begin(int, 0);

    CO source_id = g_timeout_add(160, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", FALSE);

    co_yield(0);
    assert(event == EVENT_YELLOW);
    g_source_remove(CO source_id);

    CO source_id = g_idle_add(coroutine->cb.plain, this);
    co_yield(0);
    assert(!event);

    CO source_id = g_timeout_add(160, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", TRUE);

    co_yield(0);
    assert(event == EVENT_UNYELLOW);
    g_source_remove(CO source_id);

    CO source_id = g_idle_add(coroutine->cb.plain, this);
    co_yield(0);
    assert(!event);



    CO source_id = g_timeout_add(10, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", FALSE);

    co_yield(0);
    assert(!event);

    CO source_id = g_timeout_add(160, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", TRUE);

    co_yield(0);
    assert(!event);

    CO source_id = g_timeout_add(160, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", FALSE);

    co_yield(0);
    assert(event == EVENT_YELLOW);
    g_source_remove(CO source_id);

    CO source_id = g_idle_add(coroutine->cb.plain, this);
    co_yield(0);
    assert(!event);



    yellow_shutdown(this->yellow_co);
    CO source_id = g_timeout_add(160, coroutine->cb.plain, this);
    netlink_stub_notify("eth0", TRUE);

    co_yield(0);
    assert(!event);

    g_main_loop_quit(this->mainloop);
    return 0;

    co_end;
}

static gboolean test_co(gpointer data) {
    TestCoroutine *this = data;
    Coroutine *coroutine = &this->coroutine;

    co_enter(coroutine, _test_co(coroutine, this, 0));
    return G_SOURCE_REMOVE;
}

static gboolean test_co_wrap(
        G_GNUC_UNUSED GIOChannel *channel,
        G_GNUC_UNUSED GIOCondition revents,
        gpointer data) {
    return test_co(data);
}

static void _test_queue_event(TestCoroutine *this, ColodEvent event) {
    Coroutine *coroutine = &this->coroutine;

    assert(event == EVENT_YELLOW || event == EVENT_UNYELLOW);
    co_enter(coroutine, _test_co(coroutine, this, event));
}

static void test_queue_event(gpointer data, ColodEvent event) {
    TestCoroutine *this = data;

    _test_queue_event(this, event);
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv) {
    GError *local_errp = NULL;
    TestCoroutine _this = {0};
    TestCoroutine *this = &_this;
    Coroutine *coroutine = &this->coroutine;
    coroutine->cb.iofunc = test_co_wrap;
    coroutine->cb.plain = test_co;

    this->mainloop = g_main_loop_new(g_main_context_default(), FALSE);

    this->cpg = colod_open_cpg(NULL, NULL);
    this->ctx.monitor_interface = "eth0";
    this->yellow_co = yellow_coroutine_new(this->cpg, &this->ctx,
                                           50, 100, &local_errp);
    if (!this->yellow_co) {
        colod_syslog(LOG_ERR, "yellow_coroutine_new(): %s",
                     local_errp->message);
        g_error_free(local_errp);
        return -1;
    }

    g_idle_add(coroutine->cb.plain, this);
    yellow_add_notify(this->yellow_co, test_queue_event, this);

    g_main_loop_run(this->mainloop);
    g_main_loop_unref(this->mainloop);

    yellow_coroutine_free(this->yellow_co);
    cpg_free(this->cpg);

    return 0;
}
