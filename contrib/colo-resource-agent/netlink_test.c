/*
 * COLO background netlink test
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "netlink.h"
#include "util.h"
#include "daemon.h"

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

gboolean timeout_cb(gpointer data) {
    GMainLoop *mainloop = data;
    g_main_loop_quit(mainloop);
    return G_SOURCE_REMOVE;
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv) {
    int ret;
    GError *errp = NULL;
    ColodNetlink *link;
    GMainLoop *mainloop;

    mainloop = g_main_loop_new(g_main_context_default(), FALSE);
    g_timeout_add(10, timeout_cb, mainloop);

    link = netlink_new(&errp);
    if (!link) {
        colod_syslog(LOG_ERR, "netlink_new(): %s", errp->message);
        g_error_free(errp);
        return -1;
    }

    ret = netlink_request_status(link, &errp);
    if (ret < 0) {
        colod_syslog(LOG_ERR, "netlink_request_status(): %s", errp->message);
        g_error_free(errp);
        return -1;
    }

    g_main_loop_run(mainloop);
    g_main_loop_unref(mainloop);

    netlink_free(link);

    return 0;
}
