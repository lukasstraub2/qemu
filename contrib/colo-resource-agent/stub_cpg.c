/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>

#include "cpg.h"
#include "daemon.h"

struct Cpg {
    ColodCallbackHead callbacks;
};

void colod_cpg_add_notify(Cpg *this, CpgCallback _func, gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_add(&this->callbacks, func, user_data);
}

void colod_cpg_del_notify(Cpg *this, CpgCallback _func, gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_del(&this->callbacks, func, user_data);
}

void colod_cpg_stub_notify(Cpg *this, ColodMessage message,
                           gboolean message_from_this_node,
                           gboolean peer_left_group) {
    ColodCallback *entry, *next_entry;
    QLIST_FOREACH_SAFE(entry, &this->callbacks, next, next_entry) {
        CpgCallback func = (CpgCallback) entry->func;
        func(entry->user_data, message, message_from_this_node, peer_left_group);
    }
}

void colod_cpg_send(G_GNUC_UNUSED Cpg *cpg, G_GNUC_UNUSED uint32_t message) {}

Cpg *colod_open_cpg(G_GNUC_UNUSED ColodContext *ctx, G_GNUC_UNUSED GError **errp) {
    return g_new0(Cpg, 1);
}

Cpg *cpg_new(Cpg *cpg, G_GNUC_UNUSED GError **errp) {
    return cpg;
}

void cpg_free(Cpg *cpg) {
    colod_callback_clear(&cpg->callbacks);
    g_free(cpg);
}
