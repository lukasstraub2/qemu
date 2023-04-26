/*
 * Simple filter that drops all packets.
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "net/filter.h"
#include "qemu/iov.h"

#define TYPE_FILTER_DROP "filter-drop"
DECLARE_INSTANCE_CHECKER(NetFilterState, FILTER_DROP, TYPE_FILTER_DROP)

static ssize_t filter_drop_receive_iov(NetFilterState *nf,
                                       NetClientState *sender,
                                       unsigned flags,
                                       const struct iovec *iov,
                                       int iovcnt,
                                       NetPacketSent *sent_cb)
{
    return iov_size(iov, iovcnt);
}

static void filter_drop_class_init(ObjectClass *oc, void *data)
{
    NetFilterClass *nfc = NETFILTER_CLASS(oc);

    nfc->receive_iov = filter_drop_receive_iov;
}

static const TypeInfo filter_drop_info = {
    .name = TYPE_FILTER_DROP,
    .parent = TYPE_NETFILTER,
    .class_init = filter_drop_class_init,
    .instance_size = sizeof(NetFilterState),
};

static void register_types(void)
{
    type_register_static(&filter_drop_info);
}

type_init(register_types);
