/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Shared interactive shell for Espressif platforms.
 * Provides the chat-first REPL: direct input → AI, /commands → system.
 */

#ifndef CLAW_PLATFORM_ESP_SHELL_H
#define CLAW_PLATFORM_ESP_SHELL_H

#ifdef CONFIG_RTCLAW_SHELL_ENABLE

/*
 * Run the interactive chat shell (does not return).
 *
 * Initializes the serial console, allocates the reply buffer,
 * and enters the read-eval-print loop with full line editing
 * (arrows, backspace, delete, home/end, tab completion, UTF-8).
 */
void esp_shell_loop(void);

#endif /* CONFIG_RTCLAW_SHELL_ENABLE */

#endif /* CLAW_PLATFORM_ESP_SHELL_H */
