/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Lightweight shell command framework — table-driven dispatch.
 * Each platform defines its own command table using SHELL_CMD().
 */

#ifndef CLAW_SHELL_CMD_H
#define CLAW_SHELL_CMD_H

#include <string.h>
#include <stdio.h>

typedef void (*shell_cmd_fn)(int argc, char **argv);

typedef struct {
    const char *name;
    shell_cmd_fn handler;
    const char *help;
} shell_cmd_t;

#define SHELL_CMD(n, fn, h) { (n), (fn), (h) }

#define SHELL_CMD_COUNT(table) \
    ((int)(sizeof(table) / sizeof((table)[0])))

static inline void shell_print_help(const shell_cmd_t *table, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        printf("  %-28s %s\n", table[i].name, table[i].help);
    }
    printf("\n  Anything else is sent directly to AI.\n");
}

static inline int shell_dispatch(const shell_cmd_t *table, int count,
                                 int argc, char **argv)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(table[i].name, argv[0]) == 0) {
            table[i].handler(argc, argv);
            return 1;
        }
    }
    return 0;
}

#endif /* CLAW_SHELL_CMD_H */
