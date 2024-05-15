/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <assert.h>
#include <stdio.h>

#include "eventqueue.h"

static void prepare(EventQueue *queue) {
    ColodEvent insert[] = {EVENT_START_MIGRATION, EVENT_YELLOW,
                           EVENT_FAILED, EVENT_QUIT};

    for (guint i = 0; i < sizeof(insert)/sizeof(insert[0]); i++) {
        eventqueue_add(queue, insert[i], NULL);
    }
}

void test_a() {
    EventQueue *queue;
    ColodEvent expect[] = {EVENT_FAILED, EVENT_QUIT,
                           EVENT_START_MIGRATION, EVENT_YELLOW};

    queue = eventqueue_new(4, EVENT_FAILED, EVENT_PEER_FAILOVER, EVENT_QUIT,
                           EVENT_AUTOQUIT, 0);

    prepare(queue);
    int ret = eventqueue_add(queue, EVENT_FAILED, NULL);
    assert(ret < 0);

    assert(eventqueue_pending_interrupt(queue));
    for (guint i = 0; i < sizeof(expect)/sizeof(expect[0]); i++) {
        assert(eventqueue_pending(queue));
        Event *event = eventqueue_remove(queue);
        assert(event->event == expect[i]);
        g_free(event);
    }
    assert(!eventqueue_remove(queue));
    assert(!eventqueue_peek(queue));

    eventqueue_free(queue);
}

void test_b() {
    EventQueue *queue;
    ColodEvent expect[] = {EVENT_START_MIGRATION, EVENT_FAILED,
                           EVENT_QUIT, EVENT_YELLOW};

    queue = eventqueue_new(4, EVENT_FAILED, EVENT_PEER_FAILOVER, EVENT_QUIT,
                           EVENT_AUTOQUIT, 0);

    prepare(queue);
    int ret = eventqueue_add(queue, EVENT_FAILED, NULL);
    assert(ret < 0);

    eventqueue_set_interrupting(queue, EVENT_START_MIGRATION, 0);

    assert(eventqueue_pending_interrupt(queue));
    for (guint i = 0; i < sizeof(expect)/sizeof(expect[0]); i++) {
        assert(eventqueue_pending(queue));
        Event *event = eventqueue_remove(queue);
        assert(event->event == expect[i]);
        g_free(event);
    }
    assert(!eventqueue_remove(queue));
    assert(!eventqueue_peek(queue));

    eventqueue_free(queue);
}

void test_c() {
    EventQueue *queue;
    queue = eventqueue_new(4, EVENT_FAILED, EVENT_PEER_FAILOVER, EVENT_QUIT,
                           EVENT_AUTOQUIT, 0);

    prepare(queue);

    for (int i = 0; i < 2; i++) {
        Event *event = eventqueue_remove(queue);
        g_free(event);
    }

    prepare(queue);

    eventqueue_free(queue);
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv) {
    test_a();
    test_b();
    test_c();

    return 0;
}
