/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>

#include <stdarg.h>
#include <assert.h>

#include "eventqueue.h"

struct EventQueue {
    GSequence *sequence;

    guint64 seq_counter;
    guint size;
    guint used;

    gboolean interrupting_always[EVENT_MAX];
    gboolean interrupting[EVENT_MAX];
};

static gint eventqueue_sort(gconstpointer _a, gconstpointer _b,
                            gpointer user_data) {
    EventQueue *this = user_data;
    const Event *a = _a;
    const Event *b = _b;

    if (this->interrupting[a->event] != this->interrupting[b->event]) {
        return (this->interrupting[a->event] ? -1 : 1);
    } else {
        return a->seqno - b->seqno;
    }
}

void eventqueue_set_interrupting(EventQueue *this, ...) {
    va_list args;

    memcpy(this->interrupting, this->interrupting_always,
           sizeof(this->interrupting));

    va_start(args, this);
    while (TRUE) {
        ColodEvent event = va_arg(args, ColodEvent);
        if (!event) {
            break;
        }

        this->interrupting[event] = TRUE;
    }
    va_end(args);

    g_sequence_sort(this->sequence, eventqueue_sort, this);
}

int eventqueue_add(EventQueue *this, ColodEvent _event, gpointer data) {
    Event *event;

    assert(_event && _event != EVENT_MAX);

    if (this->used == this->size) {
        return -1;
    }

    event = g_new(Event, 1);

    event->event = _event;
    event->data = data;
    event->seqno = this->seq_counter++;
    g_sequence_insert_sorted(this->sequence, event, eventqueue_sort, this);

    this->used++;
    return 0;
}

Event *eventqueue_remove(EventQueue *this) {
    Event *event;
    GSequenceIter *iter;

    if (!this->used) {
        return NULL;
    }

    iter = g_sequence_get_begin_iter(this->sequence);
    event = g_sequence_get(iter);
    g_sequence_remove(iter);

    this->used--;
    return event;
}

const Event *eventqueue_peek(EventQueue *this) {
    Event *event;
    GSequenceIter *iter;

    if (!this->used) {
        return NULL;
    }

    iter = g_sequence_get_begin_iter(this->sequence);
    event = g_sequence_get(iter);

    return event;
}

const Event *eventqueue_last(EventQueue *this) {
    Event *event;
    GSequenceIter *iter;

    if (!this->used) {
        return NULL;
    }

    iter = g_sequence_get_end_iter(this->sequence);
    iter = g_sequence_iter_prev(iter);

    event = g_sequence_get(iter);

    return event;
}

gboolean eventqueue_pending(EventQueue *this) {
    return this->used;
}

gboolean eventqueue_pending_interrupt(EventQueue *this) {
    const Event *event;

    event = eventqueue_peek(this);
    if (!event) {
        return FALSE;
    }

    return this->interrupting[event->event];
}

gboolean eventqueue_event_interrupting(EventQueue *this, ColodEvent event) {
    return this->interrupting[event];
}

EventQueue *eventqueue_new(guint size, ...){
    va_list args;
    EventQueue *this;

    this = g_new0(EventQueue, 1);
    this->sequence = g_sequence_new(NULL);
    this->size = size;

    va_start(args, size);
    while (TRUE) {
        ColodEvent event = va_arg(args, ColodEvent);
        if (!event) {
            break;
        }

        this->interrupting_always[event] = TRUE;
        this->interrupting[event] = TRUE;
    }
    va_end(args);

    return this;
}

void eventqueue_free(EventQueue *this) {
    GSequenceIter *iter;

    iter = g_sequence_get_begin_iter(this->sequence);
    while (!g_sequence_iter_is_end(iter)) {
        g_free(g_sequence_get(iter));
        g_sequence_remove(iter);
    }

    g_sequence_free(this->sequence);
    g_free(this);
}
