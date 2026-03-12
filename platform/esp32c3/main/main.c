/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 *
 * Platform entry for ESP32-C3 / ESP-IDF + FreeRTOS.
 * Chat-first shell: direct input goes to AI, /commands for system.
 */

#include "claw_os.h"
#include "claw_init.h"
#include "ai_engine.h"
#include "ai_memory.h"
#include "ai_skill.h"
#include "scheduler.h"
#include "swarm.h"

#include <stdio.h>
#include <string.h>

#ifdef CLAW_PLATFORM_ESP_IDF
#include "driver/uart.h"
#include "nvs_flash.h"
#endif

#define TAG         "main"
#define REPLY_SIZE  4096
#define INPUT_SIZE  256
#define MAX_ARGS    8

#ifdef CLAW_PLATFORM_ESP_IDF

static char *s_reply;

/*
 * Read one line from UART with raw byte echo.
 * UTF-8 multi-byte characters (Chinese etc.) pass through correctly.
 */
static int uart_read_line(char *buf, int size)
{
    int pos = 0;
    uint8_t ch;

    while (pos < size - 1) {
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY);

        if (n <= 0) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
            /* Consume trailing \n after \r (or vice versa) */
            uint8_t trail;
            uart_read_bytes(UART_NUM_0, &trail, 1, pdMS_TO_TICKS(20));
            break;
        }
        if (ch == '\b' || ch == 127) {
            if (pos > 0) {
                pos--;
                while (pos > 0 && (buf[pos] & 0xC0) == 0x80) {
                    pos--;
                }
                uart_write_bytes(UART_NUM_0, "\b \b", 3);
            }
            continue;
        }
        buf[pos++] = (char)ch;
        uart_write_bytes(UART_NUM_0, &ch, 1);
    }
    buf[pos] = '\0';
    return pos;
}

/* Split string into argv-style tokens (modifies input in place) */
static int tokenize(char *line, char **argv, int max_args)
{
    int argc = 0;

    while (*line && argc < max_args) {
        while (*line == ' ') {
            line++;
        }
        if (*line == '\0') {
            break;
        }
        argv[argc++] = line;
        while (*line && *line != ' ') {
            line++;
        }
        if (*line) {
            *line++ = '\0';
        }
    }
    return argc;
}

/* ---- Slash command handlers ---- */

static void cmd_help(void)
{
    printf("  /help                    Show this help\n");
    printf("  /history                 Show conversation message count\n");
    printf("  /clear                   Clear conversation memory\n");
    printf("  /skill [name] [args]     List or execute a skill\n");
    printf("  /sched                   List scheduled tasks\n");
    printf("  /nodes                   Show swarm node table\n");
    printf("  /remember <key> <val>    Save to long-term memory\n");
    printf("  /forget <key>            Delete from long-term memory\n");
    printf("  /memories                List long-term memories\n");
    printf("\n  Anything else is sent directly to AI.\n");
}

static void cmd_history(void)
{
    printf("Conversation memory: %d messages\n", ai_memory_count());
}

static void cmd_clear(void)
{
    ai_memory_clear();
    printf("Conversation memory cleared.\n");
}

static void cmd_sched(void)
{
    sched_list();
}

static void cmd_skill(int argc, char **argv)
{
    if (argc < 2) {
        ai_skill_list();
        return;
    }

    char params[256] = "";
    int off = 0;

    for (int i = 2; i < argc && off < (int)sizeof(params) - 1; i++) {
        if (i > 2) {
            params[off++] = ' ';
        }
        off += snprintf(params + off, sizeof(params) - off, "%s", argv[i]);
    }

    if (ai_skill_execute(argv[1], params, s_reply, REPLY_SIZE) == CLAW_OK) {
        printf("\nSkill [%s]> %s\n", argv[1], s_reply);
    } else {
        printf("\n[error] %s\n", s_reply);
    }
}

static void cmd_remember(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: /remember <key> <value...>\n");
        return;
    }

    char value[128] = "";
    int off = 0;

    for (int i = 2; i < argc && off < (int)sizeof(value) - 1; i++) {
        if (i > 2) {
            value[off++] = ' ';
        }
        off += snprintf(value + off, sizeof(value) - off, "%s", argv[i]);
    }

    if (ai_ltm_save(argv[1], value) == CLAW_OK) {
        printf("Remembered: %s = %s\n", argv[1], value);
    } else {
        printf("[error] failed to save\n");
    }
}

static void cmd_forget(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: /forget <key>\n");
        return;
    }

    if (ai_ltm_delete(argv[1]) == CLAW_OK) {
        printf("Forgot: %s\n", argv[1]);
    } else {
        printf("[error] key '%s' not found\n", argv[1]);
    }
}

static void cmd_memories(void)
{
    ai_ltm_list();
}

static void cmd_nodes(void)
{
    swarm_list_nodes();
}

/* Dispatch a /command */
static void dispatch_command(char *line)
{
    char *argv[MAX_ARGS];
    int argc = tokenize(line, argv, MAX_ARGS);

    if (argc == 0) {
        return;
    }

    const char *cmd = argv[0];

    if (strcmp(cmd, "/help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "/history") == 0) {
        cmd_history();
    } else if (strcmp(cmd, "/clear") == 0) {
        cmd_clear();
    } else if (strcmp(cmd, "/sched") == 0) {
        cmd_sched();
    } else if (strcmp(cmd, "/skill") == 0) {
        cmd_skill(argc, argv);
    } else if (strcmp(cmd, "/remember") == 0) {
        cmd_remember(argc, argv);
    } else if (strcmp(cmd, "/forget") == 0) {
        cmd_forget(argc, argv);
    } else if (strcmp(cmd, "/memories") == 0) {
        cmd_memories();
    } else if (strcmp(cmd, "/nodes") == 0) {
        cmd_nodes();
    } else {
        printf("Unknown command: %s (type /help)\n", cmd);
    }
}

/* Chat with AI (direct input) */
static void do_chat(const char *msg)
{
    if (ai_chat(msg, s_reply, REPLY_SIZE) == CLAW_OK) {
        printf("\nrt-claw> %s\n", s_reply);
    } else {
        printf("\n[error] %s\n", s_reply);
    }
}

static void shell_loop(void)
{
    char input[INPUT_SIZE];

    /* Install UART driver (required for uart_read_bytes) */
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    s_reply = claw_malloc(REPLY_SIZE);
    if (!s_reply) {
        CLAW_LOGE(TAG, "no memory for reply buffer");
        return;
    }

    /* Test AI connectivity */
    printf("\n  [boot] Testing AI connection ...\n");
    if (ai_chat_raw("Report your status in one short sentence. "
                     "Include your name, platform, and that you are online.",
                     s_reply, REPLY_SIZE) == CLAW_OK) {
        printf("  [boot] AI> %s\n", s_reply);
    } else {
        printf("  [boot] AI test failed: %s\n", s_reply);
    }

    printf("\n");
    printf("  rt-claw chat  (type /help for commands)\n");
    printf("  Direct input sends to AI, /command for system.\n");
    printf("\n");

    while (1) {
        printf("\nYou> ");
        int len = uart_read_line(input, sizeof(input));

        if (len == 0) {
            continue;
        }

        if (input[0] == '/') {
            dispatch_command(input);
        } else {
            do_chat(input);
        }
    }
}

#endif /* CLAW_PLATFORM_ESP_IDF */

void app_main(void)
{
#ifdef CLAW_PLATFORM_ESP_IDF
    /* Initialize NVS flash for long-term memory storage */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#endif

    claw_init();

#ifdef CLAW_PLATFORM_ESP_IDF
    shell_loop();
#endif

    /* Keep main task alive */
    while (1) {
        claw_thread_delay_ms(1000);
    }
}
