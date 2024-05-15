/*
 * COLO background daemon coroutine utils
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "coutil.h"
#include "util.h"
#include "daemon.h"

#include <glib-2.0/glib.h>

#include "coroutine_stack.h"

int _colod_channel_read_line_timeout_co(Coroutine *coroutine,
                                        GIOChannel *channel,
                                        gchar **line,
                                        gsize *len,
                                        guint timeout,
                                        GError **errp) {
    struct {
        guint timeout_source_id, io_source_id;
    } *co;

    co_frame(co, sizeof(*co));
    co_begin(int, 0);

    if (timeout) {
        CO timeout_source_id = g_timeout_add(timeout, coroutine->cb.plain,
                                             coroutine);
        g_source_set_name_by_id(CO timeout_source_id, "channel read timeout");
    }

    while (TRUE) {
        GIOStatus ret = g_io_channel_read_line(channel, line, len, NULL, errp);

        if ((ret == G_IO_STATUS_NORMAL && *len == 0) ||
                ret == G_IO_STATUS_AGAIN) {
            CO io_source_id = g_io_add_watch(channel,
                                             G_IO_IN | G_IO_HUP,
                                             coroutine->cb.iofunc,
                                             coroutine);
            g_source_set_name_by_id(CO io_source_id, "channel read io watch");
            co_yield_int(G_SOURCE_REMOVE);

            guint source_id = g_source_get_id(g_main_current_source());
            if (timeout && source_id == CO timeout_source_id) {
                g_source_remove(CO io_source_id);
                g_set_error(errp, COLOD_ERROR, COLOD_ERROR_TIMEOUT,
                            "Channel read timed out");
                goto err;
            } else if (source_id != CO io_source_id) {
                g_source_remove(CO io_source_id);
                colod_trace("%s:%u: Got woken by unknown source\n",
                            __func__, __LINE__);
            }
        } else if (ret == G_IO_STATUS_NORMAL) {
            break;
        } else if (ret == G_IO_STATUS_ERROR) {
            goto err;
        } else if (ret == G_IO_STATUS_EOF) {
            g_set_error(errp, COLOD_ERROR, COLOD_ERROR_EOF, "Channel got EOF");
            goto err;
        }
    }

    if (timeout) {
        g_source_remove(CO timeout_source_id);
    }
    co_end;

    return 0;

err:
    if (timeout) {
        g_source_remove(CO timeout_source_id);
    }

    return -1;
}

int _colod_channel_read_line_co(Coroutine *coroutine,
                                GIOChannel *channel, gchar **line,
                                gsize *len, GError **errp) {
    return _colod_channel_read_line_timeout_co(coroutine, channel, line,
                                               len, 0, errp);
}

int _colod_channel_write_timeout_co(Coroutine *coroutine,
                                    GIOChannel *channel,
                                    const gchar *buf,
                                    gsize len,
                                    guint timeout,
                                    GError **errp) {
    struct {
        guint timeout_source_id, io_source_id;
        gsize offset;
    } *co;
    gsize write_len;

    co_frame(co, sizeof(*co));
    co_begin(int, 0);

    if (timeout) {
        CO timeout_source_id = g_timeout_add(timeout, coroutine->cb.plain,
                                             coroutine);
        g_source_set_name_by_id(CO timeout_source_id, "channel write timeout");
    }

    CO offset = 0;
    while (CO offset < len) {
        GIOStatus ret = g_io_channel_write_chars(channel,
                                                 buf + CO offset,
                                                 len - CO offset,
                                                 &write_len, errp);
        CO offset += write_len;

        if (ret == G_IO_STATUS_NORMAL || ret == G_IO_STATUS_AGAIN) {
            if (write_len == 0) {
                CO io_source_id = g_io_add_watch(channel, G_IO_OUT | G_IO_HUP,
                                                 coroutine->cb.iofunc,
                                                 coroutine);
                g_source_set_name_by_id(CO io_source_id, "channel write io watch");
                co_yield_int(G_SOURCE_REMOVE);

                guint source_id = g_source_get_id(g_main_current_source());
                if (timeout && source_id == CO timeout_source_id) {
                    g_source_remove(CO io_source_id);
                    g_set_error(errp, COLOD_ERROR, COLOD_ERROR_TIMEOUT,
                                "Channel write timed out");
                    goto err;
                } else if (source_id != CO io_source_id) {
                    g_source_remove(CO io_source_id);
                    colod_trace("%s:%u: Got woken by unknown source\n",
                                __func__, __LINE__);
                }
            }
        } else if (ret == G_IO_STATUS_ERROR) {
            goto err;
        } else if (ret == G_IO_STATUS_EOF) {
            g_set_error(errp, COLOD_ERROR, COLOD_ERROR_EOF, "Channel got EOF");
            goto err;
        }
    }

    while (TRUE) {
        GIOStatus ret = g_io_channel_flush(channel, errp);

        if (ret == G_IO_STATUS_AGAIN) {
            CO io_source_id = g_io_add_watch(channel, G_IO_OUT | G_IO_HUP,
                                             coroutine->cb.iofunc,
                                             coroutine);
            g_source_set_name_by_id(CO io_source_id, "channel flush io watch");
            co_yield_int(G_SOURCE_REMOVE);

            guint source_id = g_source_get_id(g_main_current_source());
            if (timeout && source_id == CO timeout_source_id) {
                g_source_remove(CO io_source_id);
                g_set_error(errp, COLOD_ERROR, COLOD_ERROR_TIMEOUT,
                            "Channel write timed out");
                goto err;
            } else if (source_id != CO io_source_id) {
                g_source_remove(CO io_source_id);
                colod_trace("%s:%u: Got woken by unknown source\n",
                            __func__, __LINE__);
            }
        } else if (ret == G_IO_STATUS_NORMAL) {
            break;
        } else if (ret == G_IO_STATUS_ERROR) {
            goto err;
        } else if (ret == G_IO_STATUS_EOF) {
            g_set_error(errp, COLOD_ERROR, COLOD_ERROR_EOF, "Channel got EOF");
            goto err;
        }
    }

    if (timeout) {
        g_source_remove(CO timeout_source_id);
    }

    co_end;

    return 0;

err:
    if (timeout) {
        g_source_remove(CO timeout_source_id);
    }

    return -1;
}

int _colod_channel_write_co(Coroutine *coroutine,
                            GIOChannel *channel, const gchar *buf,
                            gsize len, GError **errp) {
    return _colod_channel_write_timeout_co(coroutine, channel, buf, len, 0,
                                           errp);
}
