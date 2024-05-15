/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QMPCOMMANDS_H
#define QMPCOMMANDS_H

#include <glib-2.0/glib.h>
#include <json-glib-1.0/json-glib/json-glib.h>

typedef struct QmpCommands {
    JsonNode *migration_start, *migration_switchover;
    JsonNode *failover_primary, *failover_secondary;
} QmpCommands;

int qmp_commands_set_migration_start(QmpCommands *this, JsonNode *commands);
int qmp_commands_set_migration_switchover(QmpCommands *this, JsonNode *commands);
int qmp_commands_set_failover_primary(QmpCommands *this, JsonNode *commands);
int qmp_commands_set_failover_secondary(QmpCommands *this, JsonNode *commands);

QmpCommands *qmp_commands_new();
void qmp_commands_free(QmpCommands *commands);

#endif // QMPCOMMANDS_H
