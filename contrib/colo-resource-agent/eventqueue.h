/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <glib-2.0/glib.h>

typedef enum ColodEvent ColodEvent;

enum ColodEvent {
    EVENT_FAILED = 1,
    EVENT_PEER_FAILOVER,
    EVENT_QUIT,
    EVENT_AUTOQUIT,

    EVENT_FAILOVER_SYNC,

    EVENT_FAILOVER_WIN,
    EVENT_YELLOW,
    EVENT_UNYELLOW,
    EVENT_START_MIGRATION,
    EVENT_MAX
};

typedef struct Event Event;
typedef struct EventQueue EventQueue;

struct Event {
    ColodEvent event;
    gpointer data;
    guint64 seqno;
};

void eventqueue_set_interrupting(EventQueue *this, ...);

int eventqueue_add(EventQueue *this, ColodEvent _event, gpointer data);
Event *eventqueue_remove(EventQueue *this);
const Event *eventqueue_peek(EventQueue *this);
const Event *eventqueue_last(EventQueue *this);

gboolean eventqueue_pending(EventQueue *this);
gboolean eventqueue_pending_interrupt(EventQueue *this);
gboolean eventqueue_event_interrupting(EventQueue *this, ColodEvent event);

EventQueue *eventqueue_new(guint size, ...);
void eventqueue_free(EventQueue *this);

char *null_stack();

#endif // EVENTQUEUE_H
