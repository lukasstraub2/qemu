/*
 * Utilities
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

// For O_CLOEXEC
#define _GNU_SOURCE

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

G_DEFINE_QUARK(colod-error-quark, colod_error)

int colod_unix_connect(gchar *path, GError **errp) {
    struct sockaddr_un address = { 0 };
    int ret, fd;

    if (strlen(path) >= sizeof(address.sun_path)) {
        colod_error_set(errp, "Unix path too long");
        return -1;
    }
    strcpy(address.sun_path, path);
    address.sun_family = AF_UNIX;

    ret = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ret < 0) {
        colod_error_set(errp, "Failed to create socket: %s",
                        g_strerror(errno));
        return -1;
    }
    fd = ret;

    ret = connect(fd, (const struct sockaddr *) &address,
                  sizeof(address));
    if (ret < 0) {
        colod_error_set(errp, "Failed to connect socket: %s",
                        g_strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int colod_fd_set_blocking(int fd, gboolean blocking, GError **errp) {
    int flags, ret;

    ret = fcntl(fd, F_GETFL, 0);
    if (ret < 0) {
        colod_error_set(errp, "Failed to get file flags: %s",
                        g_strerror(errno));
        return -1;
    }
    flags = ret;

    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    ret = fcntl(fd, F_SETFL, flags);
    if (ret < 0) {
        colod_error_set(errp, "Failed to set file flags: %s",
                        g_strerror(errno));
        return -1;
    }

    return 0;
}

static gboolean progress_source_prepare(G_GNUC_UNUSED GSource *source,
                                        gint *timeout)
{
    *timeout = -1;

    return FALSE;
}

static gboolean progress_source_check(G_GNUC_UNUSED GSource *source)
{
    return TRUE;
}

static gboolean progress_source_dispatch(G_GNUC_UNUSED GSource *source,
                                         GSourceFunc callback,
                                         gpointer user_data)
{
    return (*callback)(user_data);
}

static void progress_source_finalize(G_GNUC_UNUSED GSource *source)
{
    // NOOP
}

GSourceFuncs progress_source_funcs = {
    progress_source_prepare,
    progress_source_check,
    progress_source_dispatch,
    progress_source_finalize,
    NULL, NULL
};

guint progress_source_add(GSourceFunc func, gpointer data) {
    GMainContext *context = g_main_context_default();
    GSource *source;
    guint source_id;

    source = g_source_new(&progress_source_funcs, sizeof(GSource));
    g_source_set_priority(source, G_PRIORITY_DEFAULT_IDLE);
    g_source_set_callback(source, func, data, NULL);
    source_id = g_source_attach(source, context);
    g_source_unref(source);
    return source_id;
}

GIOChannel *colod_create_channel(int fd, GError **errp) {
    GError *local_errp = NULL;
    GIOChannel *channel;

    channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(channel, NULL, &local_errp);
    if (local_errp) {
        colod_error_set(errp, "Failed to set channel encoding: %s",
                        local_errp->message);
        g_error_free(local_errp);
        g_io_channel_unref(channel);
        return NULL;
    }

    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, &local_errp);
    if (local_errp) {
        colod_error_set(errp, "Failed to set channel nonblocking: %s",
                        local_errp->message);
        g_error_free(local_errp);
        g_io_channel_unref(channel);
        return NULL;
    }
    g_io_channel_set_close_on_unref(channel, TRUE);

    return channel;
}

void colod_shutdown_channel(GIOChannel *channel) {
    int fd = g_io_channel_unix_get_fd(channel);
    shutdown(fd, SHUT_RDWR);
}

static guint queue_size() {
    ColodQueue queue;
    return sizeof(queue.queue)/sizeof(queue.queue[0]);
}

gboolean queue_empty(ColodQueue *queue) {
    return queue->read_pos == queue->write_pos;
}

void queue_add(ColodQueue *queue, guint entry) {
    guint size = queue_size();
    guint next_pos = (queue->write_pos + 1) % size;

    if (next_pos == queue->read_pos) {
        // queue full
        return;
    }

    queue->queue[queue->write_pos] = entry;
    queue->write_pos = next_pos;
}

guint queue_peek(ColodQueue *queue) {
    assert(!queue_empty(queue));

    return queue->queue[queue->read_pos];
}

guint queue_remove(ColodQueue *queue) {
    guint size = queue_size();
    guint ret;

    ret = queue_peek(queue);
    queue->read_pos = (queue->read_pos + 1) % size;
    return ret;
}

ColodCallback *colod_callback_find(ColodCallbackHead *head,
                                   ColodCallbackFunc func, gpointer user_data) {
    ColodCallback *entry;
    QLIST_FOREACH(entry, head, next) {
        if (entry->func == func && entry->user_data == user_data) {
            return entry;
        }
    }

    return NULL;
}

void colod_callback_add(ColodCallbackHead *head,
                        ColodCallbackFunc func, gpointer user_data) {
    ColodCallback *cb;

    assert(!colod_callback_find(head, func, user_data));

    cb = g_new0(ColodCallback, 1);
    cb->func = func;
    cb->user_data = user_data;

    QLIST_INSERT_HEAD(head, cb, next);
}

void colod_callback_del(ColodCallbackHead *head,
                        ColodCallbackFunc func, gpointer user_data) {
    ColodCallback *cb;

    cb = colod_callback_find(head, func, user_data);
    assert(cb);

    QLIST_REMOVE(cb, next);
    g_free(cb);
}

void colod_callback_clear(ColodCallbackHead *head) {
    while (!QLIST_EMPTY(head)) {
        ColodCallback *cb = QLIST_FIRST(head);
        QLIST_REMOVE(cb, next);
        g_free(cb);
    }
}

const char *colod_source_name_or_null(GSource *source) {
    const char *ret;

    if (!source) {
        return "NULL";
    }

    ret = g_source_get_name(source);
    return (ret? ret : "NULL");
}

void _colod_assert_remove_one_source(gpointer data, const gchar *func,
                                     int line) {
    GMainContext *mainctx = g_main_context_default();
    char *first_source;
    GSource *tmp;

    tmp = g_main_context_find_source_by_user_data(mainctx, data);
    if (!tmp) {
        return;
    }

    first_source = g_strdup(colod_source_name_or_null(tmp));
    assert(g_source_remove_by_user_data(data));

    tmp = g_main_context_find_source_by_user_data(mainctx, data);
    if (!tmp) {
        g_free(first_source);
        return;
    }

    gboolean print_header = TRUE;
    while (TRUE) {
        tmp = g_main_context_find_source_by_user_data(mainctx, data);
        if (!tmp) {
            break;
        }

        const char *source = colod_source_name_or_null(tmp);
        if (print_header) {
            fprintf(stderr,
                    "%s:%u: More than one source: first source: \"%s\", "
                    "additional source: \"%s\"\n",
                    func, line, first_source, source);
            print_header = FALSE;
        } else {
            fprintf(stderr, "%s:%u: Even more sources: \"%s\"\n",
                    func, line, source);
        }
        assert(g_source_remove_by_user_data(data));
    }

    abort();
}
