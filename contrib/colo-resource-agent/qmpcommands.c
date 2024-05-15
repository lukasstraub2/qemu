/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <glib-2.0/glib.h>
#include <json-glib-1.0/json-glib/json-glib.h>

#include <assert.h>

#include "qmpcommands.h"

int qmp_commands_set_migration_start(QmpCommands *this, JsonNode *commands) {
    if (this->migration_start) {
        json_node_unref(this->migration_start);
    }

    this->migration_start = json_node_ref(commands);
    return 0;
}

int qmp_commands_set_migration_switchover(QmpCommands *this, JsonNode *commands) {
    if (this->migration_switchover) {
        json_node_unref(this->migration_switchover);
    }

    this->migration_switchover = json_node_ref(commands);
    return 0;
}

int qmp_commands_set_failover_primary(QmpCommands *this, JsonNode *commands) {
    if (this->failover_primary) {
        json_node_unref(this->failover_primary);
    }

    this->failover_primary = json_node_ref(commands);
    return 0;
}

int qmp_commands_set_failover_secondary(QmpCommands *this, JsonNode *commands) {
    if (this->failover_secondary) {
        json_node_unref(this->failover_secondary);
    }

    this->failover_secondary = json_node_ref(commands);
    return 0;
}

QmpCommands *qmp_commands_new() {
    QmpCommands *this;

    this = g_new0(QmpCommands, 1);
    this->migration_start = json_from_string("[]", NULL);
    assert(this->migration_start);
    this->migration_switchover = json_from_string("[]", NULL);
    assert(this->migration_switchover);
    this->failover_primary = json_from_string("[]", NULL);
    assert(this->failover_primary);
    this->failover_secondary = json_from_string("[]", NULL);
    assert(this->failover_secondary);

    return this;
}

void qmp_commands_free(QmpCommands *this) {
    if (this->migration_start) {
        json_node_unref(this->migration_start);
    }
    if (this->migration_switchover) {
        json_node_unref(this->migration_switchover);
    }
    if (this->failover_primary) {
        json_node_unref(this->failover_primary);
    }
    if (this->failover_secondary) {
        json_node_unref(this->failover_secondary);
    }

    g_free(this);
}
