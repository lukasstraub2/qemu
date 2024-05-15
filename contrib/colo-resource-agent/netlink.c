/*
 * COLO background netlink
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>
#include <glib-2.0/glib-unix.h>

#include <netlink/netlink.h>
#include <netlink/msg.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>

#include "util.h"
#include "daemon.h"
#include "netlink.h"

struct ColodNetlink {
    struct nl_sock *sock;
    guint source_id;
    ColodCallbackHead callbacks;
};

void netlink_add_notify(ColodNetlink *this, NetlinkCallback _func,
                        gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_add(&this->callbacks, func, user_data);
}

void netlink_del_notify(ColodNetlink *this, NetlinkCallback _func,
                        gpointer user_data) {
    ColodCallbackFunc func = (ColodCallbackFunc) _func;
    colod_callback_del(&this->callbacks, func, user_data);
}

static void notify(ColodNetlink *this, const char *ifname, gboolean up) {
    ColodCallback *entry, *next_entry;
    QLIST_FOREACH_SAFE(entry, &this->callbacks, next, next_entry) {
        NetlinkCallback func = (NetlinkCallback) entry->func;
        func(entry->user_data, ifname, up);
    }
}

static int netlink_message(struct nl_msg *msg, void *data) {
    ColodNetlink *this = data;
    const struct nlmsghdr *hdr = nlmsg_hdr(msg);
    char ifname[IF_NAMESIZE] = {0};

    if (hdr->nlmsg_type == RTM_NEWLINK) {
        struct ifinfomsg *ifi = nlmsg_data(hdr);
        if_indextoname(ifi->ifi_index, ifname);
        colod_trace("netlink message: link %s %s\n", ifname,
                    (ifi->ifi_flags & IFF_RUNNING) ? "up" : "down");

        notify(this, ifname, ifi->ifi_flags & IFF_RUNNING);
    }

    return NL_OK;
}

static gboolean netlink_io_watch(G_GNUC_UNUSED int fd,
                                 G_GNUC_UNUSED GIOCondition condition,
                                 gpointer data) {
    int ret;
    ColodNetlink *this = data;

    ret = nl_recvmsgs_default(this->sock);
    if (ret < 0) {
        colod_syslog(LOG_ERR, "Failed processing netlink messages: %s",
                     nl_geterror(ret));
        this->source_id = 0;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

int netlink_request_status(ColodNetlink *this, GError **errp) {
    int ret;
    struct nl_msg *msg;
    struct ifinfomsg hdr = {0};
    hdr.ifi_family = AF_UNSPEC;

    msg = nlmsg_alloc_simple(RTM_GETLINK, NLM_F_DUMP);
    if (!msg) {
        log_error("Failed to allocate netlink message");
        abort();
    }

    ret = nlmsg_append(msg, &hdr, sizeof(hdr), NLMSG_ALIGNTO);
    if (ret < 0) {
        colod_error_set(errp, "Failed to build netlink message: %s",
                        nl_geterror(ret));
        goto err;
    }

    ret = nl_send_auto(this->sock, msg);
    if (ret < 0) {
        colod_error_set(errp, "Failed to send netlink message: %s",
                        nl_geterror(ret));
        goto err;
    }

    nlmsg_free(msg);
    return 0;

err:
    nlmsg_free(msg);
    return -1;
}

void netlink_free(ColodNetlink *this) {
    colod_callback_clear(&this->callbacks);

    if (this->source_id) {
        g_source_remove(this->source_id);
    }
    nl_socket_free(this->sock);
    g_free(this);
}

ColodNetlink *netlink_new(GError **errp) {
    int ret;
    struct nl_sock *sock;

    sock = nl_socket_alloc();
    if (!sock) {
        log_error("Failed to allocate netlink socket");
        abort();
    }

    ret = nl_connect(sock, NETLINK_ROUTE);
    if (ret < 0) {
        colod_error_set(errp, "Failed to connect netlink socket: %s",
                        nl_geterror(ret));
        goto err;
    }

    ret = nl_socket_add_memberships(sock, RTNLGRP_LINK, 0);
    if (ret < 0) {
        colod_error_set(errp, "Failed to join netlink mcast membership: %s",
                        nl_geterror(ret));
        goto err;
    }

    nl_socket_disable_seq_check(sock);

    ret = nl_socket_set_nonblocking(sock);
    if (ret < 0) {
        colod_error_set(errp, "Failed to set netlink socket nonblocking: %s",
                        nl_geterror(ret));
        goto err;
    }

    ColodNetlink *this = g_new0(ColodNetlink, 1);
    this->sock = sock;

    ret = nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM,
                              netlink_message, this);
    if (ret < 0) {
        colod_error_set(errp, "Failed to set netlink callback: %s",
                        nl_geterror(ret));
        g_free(this);
        goto err;
    }

    int fd = nl_socket_get_fd(sock);
    this->source_id = g_unix_fd_add(fd, G_IO_IN | G_IO_HUP,
                                    netlink_io_watch, this);

    return this;

err:
    nl_socket_free(sock);
    return NULL;
}
