/*
 * COLO background daemon netlink stub
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "netlink.h"
#include "util.h"

struct ColodNetlink {
    int dummy;
};

ColodCallbackHead callbacks;

void netlink_add_notify(G_GNUC_UNUSED ColodNetlink *this,
                        NetlinkCallback _func, gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_add(&callbacks, func, user_data);
}

void netlink_del_notify(G_GNUC_UNUSED ColodNetlink *this,
                        NetlinkCallback _func, gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_del(&callbacks, func, user_data);
}

void netlink_stub_notify(const char *ifname, gboolean up) {
    ColodCallback *entry, *next_entry;
    QLIST_FOREACH_SAFE(entry, &callbacks, next, next_entry) {
        NetlinkCallback func = (NetlinkCallback) entry->func;
        func(entry->user_data, ifname, up);
    }
}

int netlink_request_status(G_GNUC_UNUSED ColodNetlink *this,
                           G_GNUC_UNUSED GError **errp) {
    return 0;
}

void netlink_free(ColodNetlink *this) {
    colod_callback_clear(&callbacks);
    g_free(this);
}

ColodNetlink *netlink_new(G_GNUC_UNUSED GError **errp) {
    return g_new0(ColodNetlink, 1);
}
