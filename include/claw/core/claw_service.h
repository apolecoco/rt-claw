/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Service framework — OOP base class with lifecycle state machine,
 * dependency declaration, and linker section auto-registration.
 */

#ifndef CLAW_CORE_CLAW_SERVICE_H
#define CLAW_CORE_CLAW_SERVICE_H

#include "claw/core/claw_errno.h"
#include "claw/core/claw_class.h"

/* ------------------------------------------------------------------ */
/* Service lifecycle states                                           */
/* ------------------------------------------------------------------ */

enum claw_service_state {
    CLAW_SVC_CREATED,
    CLAW_SVC_INITIALIZED,
    CLAW_SVC_RUNNING,
    CLAW_SVC_STOPPING,
    CLAW_SVC_STOPPED,
};

/* ------------------------------------------------------------------ */
/* Service ops vtable                                                 */
/* ------------------------------------------------------------------ */

struct claw_service;

struct claw_service_ops {
    claw_err_t (*init)(struct claw_service *svc);
    claw_err_t (*start)(struct claw_service *svc);    /* NULL = no-op */
    void       (*stop)(struct claw_service *svc);     /* NULL = no-op */
};

/* ------------------------------------------------------------------ */
/* Service base class                                                 */
/* ------------------------------------------------------------------ */

struct claw_service {
    const char                    *name;
    const struct claw_service_ops *ops;
    const char                   **deps;    /* NULL-terminated dep names */
    enum claw_service_state        state;
    claw_list_node_t               node;    /* registry linkage */
};

/* ------------------------------------------------------------------ */
/* Service core API                                                   */
/* ------------------------------------------------------------------ */

/*
 * Register a service into the runtime registry.
 * Validates ops->init is non-NULL.
 */
claw_err_t claw_service_register(struct claw_service *svc);

/*
 * Resolve dependencies and start all registered services in
 * topological order.  Returns CLAW_ERR_CYCLE on circular deps,
 * CLAW_ERR_DEPEND on missing deps.
 */
claw_err_t claw_service_start_all(void);

/*
 * Stop all running services in reverse order.
 */
void claw_service_stop_all(void);

/*
 * Query current state of a service.
 */
enum claw_service_state claw_service_get_state(const struct claw_service *svc);

/*
 * Collect services from the linker section into the runtime registry.
 * Called once from claw_init() before claw_service_start_all().
 */
claw_err_t claw_service_collect_from_section(void);

#endif /* CLAW_CORE_CLAW_SERVICE_H */
