/*
 * Utilities
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <glib-2.0/glib.h>

#include "queue.h"

typedef enum ColodError {
    COLOD_ERROR_FATAL,
    COLOD_ERROR_TIMEOUT,
    COLOD_ERROR_QMP,
    COLOD_ERROR_EOF,
    COLOD_ERROR_INTERRUPT
} ColodError;

#define COLOD_ERROR (colod_error_quark())

#define colod_error_set(errp, fmt, ...) \
    g_set_error(errp, COLOD_ERROR, COLOD_ERROR_FATAL, \
                fmt, ##__VA_ARGS__);

GQuark colod_error_quark();

size_t colod_write_full(int fd, const uint8_t *buf, size_t count);
size_t colod_read_full(int fd, uint8_t *buf, size_t count);
gboolean colod_write_pidfile(const char *path, GError **errp);
int os_daemonize(void);
int os_daemonize_post_init(int pipe, GError **errp);

int colod_unix_connect(gchar *path, GError **errp);
int colod_fd_set_blocking(int fd, gboolean blocking, GError **errp);
guint progress_source_add(GSourceFunc func, gpointer data);
GIOChannel *colod_create_channel(int fd, GError **errp);
void colod_shutdown_channel(GIOChannel *channel);

typedef struct ColodQueue {
    guint queue[16];
    guint read_pos, write_pos;
} ColodQueue;

gboolean queue_empty(ColodQueue *queue);
void queue_add(ColodQueue *queue, guint entry);
guint queue_peek(ColodQueue *queue);
guint queue_remove(ColodQueue *queue);

typedef void (*ColodCallbackFunc)(void);
typedef struct ColodCallback {
    QLIST_ENTRY(ColodCallback) next;
    ColodCallbackFunc func;
    gpointer user_data;
} ColodCallback;

typedef QLIST_HEAD(ColodCallbackHead, ColodCallback) ColodCallbackHead;

ColodCallback *colod_callback_find(ColodCallbackHead *head,
                                   ColodCallbackFunc func, gpointer user_data);
void colod_callback_add(ColodCallbackHead *head,
                        ColodCallbackFunc func, gpointer user_data);
void colod_callback_del(ColodCallbackHead *head,
                        ColodCallbackFunc func, gpointer user_data);
void colod_callback_clear(ColodCallbackHead *head);

const char *colod_source_name_or_null(GSource *source);
#define colod_assert_remove_one_source(data) \
    _colod_assert_remove_one_source((data), __func__, __LINE__)
void _colod_assert_remove_one_source(gpointer data, const gchar *func,
                                     int line);

#endif // UTIL_H
