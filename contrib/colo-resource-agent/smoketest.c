/*
 * COLO background daemon smoketest
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <assert.h>

#include <glib-2.0/glib.h>
#include <glib-2.0/glib-unix.h>

#include "base_types.h"
#include "smoketest.h"
#include "daemon.h"
#include "main_coroutine.h"
#include "coroutine.h"
#include "coroutine_stack.h"
#include "util.h"
#include "client.h"
#include "qmp.h"
#include "cpg.h"
#include "watchdog.h"

extern FILE *trace;
extern gboolean do_syslog;

__attribute__((weak))
void colod_trace(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (trace) {
        vfprintf(stderr, fmt, args);
        fflush(stderr);
    }

    va_end(args);
}

__attribute__((weak))
void colod_syslog(G_GNUC_UNUSED int pri, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fwrite("\n", 1, 1, stderr);
    va_end(args);
}

const gchar *smoke_basedir() {
    const gchar *base_dir = g_getenv("COLOD_BASE_DIR");

    if (base_dir) {
        return base_dir;
    } else {
        mkdir("/tmp/.colod_smoke", 0700);
        return "/tmp/.colod_smoke";
    }
}

gboolean smoke_do_trace() {
    const gchar *do_trace = g_getenv("COLOD_TRACE");

    return (do_trace ? TRUE : FALSE);
}

void smoke_init() {
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    prctl(PR_SET_DUMPABLE, 1);

    signal(SIGPIPE, SIG_IGN);

    if (smoke_do_trace()) {
        trace = (FILE*) 1;
    }
}

static int socketpair_channel(GIOChannel **channel, GError **errp) {
    int ret;
    int fd[2];

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (ret < 0) {
        colod_error_set(errp, "Failed to open qmp socketpair: %s",
                        g_strerror(errno));
        return -1;
    }

    *channel = colod_create_channel(fd[0], errp);
    if (!*channel) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    return fd[1];
}

static int smoke_open_qmp(SmokeColodContext *sctx, GError **errp) {
    int ret;

    ret = socketpair_channel(&sctx->qmp_ch, errp);
    if (ret < 0) {
        return -1;
    }
    sctx->cctx.qmp_fd = ret;

    ret = socketpair_channel(&sctx->qmp_yank_ch, errp);
    if (ret < 0) {
        return -1;
    }
    sctx->cctx.qmp_yank_fd = ret;

    return 0;
}

static int smoke_open_mngmt(SmokeColodContext *sctx, GError **errp) {
    int sockfd, clientfd, ret;
    struct sockaddr_un address = { 0 };
    g_autofree char *path = NULL;

    path = g_strconcat(smoke_basedir(), "/colod.sock", NULL);
    if (strlen(path) >= sizeof(address.sun_path)) {
        colod_error_set(errp, "Management unix path too long");
        return -1;
    }
    strcpy(address.sun_path, path);
    address.sun_family = AF_UNIX;

    ret = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ret < 0) {
        colod_error_set(errp, "Failed to create management socket: %s",
                        g_strerror(errno));
        return -1;
    }
    sockfd = ret;

    unlink(path);
    ret = bind(sockfd, (const struct sockaddr *) &address, sizeof(address));
    if (ret < 0) {
        colod_error_set(errp, "Failed to bind management socket: %s",
                        g_strerror(errno));
        goto err;
    }

    ret = listen(sockfd, 2);
    if (ret < 0) {
        colod_error_set(errp, "Failed to listen management socket: %s",
                        g_strerror(errno));
        goto err;
    }

    ret = colod_fd_set_blocking(sockfd, FALSE, errp);
    if (ret < 0) {
        goto err;
    }

    sctx->cctx.mngmt_listen_fd = sockfd;

    ret = colod_unix_connect(path, errp);
    if (ret < 0) {
        colod_error_set(errp, "Failed to connect to management socket: %s",
                        g_strerror(errno));
        goto err;
    }
    clientfd = ret;

    sctx->client_ch = colod_create_channel(ret, errp);
    if (!sctx->client_ch) {
        close(clientfd);
        goto err;
    }

    return 0;

err:
    close(sockfd);
    return -1;
}

static void smoke_fill_cctx(ColodContext *cctx) {
    cctx->node_name = "tele-clu-01";
    cctx->instance_name = "colo_test";
    cctx->base_dir = "";
    cctx->qmp_path = "";
    cctx->qmp_yank_path = "";
    cctx->daemonize = FALSE;
    cctx->qmp_timeout_low = 600;
    cctx->qmp_timeout_high = 10000;
    cctx->watchdog_interval = 0;
    cctx->do_trace = smoke_do_trace();
    cctx->primary_startup = FALSE;
}

SmokeColodContext *smoke_context_new(GError **errp) {
    SmokeColodContext *sctx;
    int ret;

    sctx = g_new0(SmokeColodContext, 1);
    smoke_fill_cctx(&sctx->cctx);
    ret = smoke_open_qmp(sctx, errp);
    if (ret < 0) {
        goto err;
    }

    ret = smoke_open_mngmt(sctx, errp);
    if (ret < 0) {
        goto err;
    }

    sctx->cctx.cpg = colod_open_cpg(&sctx->cctx, errp);
    if (!sctx->cctx.cpg) {
        goto err;
    }

    return sctx;

err:
    g_free(sctx);
    return NULL;
}

void smoke_context_free(SmokeColodContext *sctx) {
    g_io_channel_unref(sctx->client_ch);
    g_io_channel_unref(sctx->qmp_ch);
    g_io_channel_unref(sctx->qmp_yank_ch);
    g_free(sctx);
}
