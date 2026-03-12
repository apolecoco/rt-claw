/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 */

#ifndef __CLAW_AI_ENGINE_H__
#define __CLAW_AI_ENGINE_H__

#include "claw_os.h"

/* Status phases for the progress callback */
#define AI_STATUS_THINKING   0  /* waiting for API response */
#define AI_STATUS_TOOL_CALL  1  /* executing a tool (detail = name) */
#define AI_STATUS_DONE       2  /* request complete */

typedef void (*ai_status_cb_t)(int status, const char *detail);

void ai_set_status_cb(ai_status_cb_t cb);

int ai_engine_init(void);

/**
 * Send a user message to the LLM and receive a reply.
 * Stores user/assistant messages in conversation memory.
 */
int ai_chat(const char *user_msg, char *reply, size_t reply_size);

/**
 * One-shot LLM call without conversation memory.
 * Used by skill system to avoid polluting main history.
 */
int ai_chat_raw(const char *prompt, char *reply, size_t reply_size);

#endif /* __CLAW_AI_ENGINE_H__ */
