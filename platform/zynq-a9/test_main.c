/* SPDX-License-Identifier: MIT */
/*
 * Unit test entry for Zynq-A9 QEMU.
 *
 * Runs tests directly in main() without the FreeRTOS scheduler.
 * Tests that require OSAL primitives (mutex/queue) are skipped
 * because they need a running scheduler — those are covered by
 * the vexpress-a9 (RT-Thread) unit tests.
 *
 * Result signaled via UART marker: ZYNQ_TEST_EXIT:PASS/FAIL.
 */

#include <stdio.h>

#include "tests/unit/framework/test.h"

/* Test suites that do NOT require OSAL / FreeRTOS scheduler */
extern int test_ai_memory_suite(void);
extern int test_im_util_suite(void);
extern int test_ota_suite(void);

int main(void)
{
    int failed = 0;

    printf("rt-claw unit tests (Zynq-A9 QEMU, FreeRTOS)\n");
    printf("\n========== rt-claw unit tests ==========\n\n");

    failed += test_ai_memory_suite();
    failed += test_im_util_suite();
    failed += test_ota_suite();

    printf("\n========================================\n");
    if (failed) {
        printf("FAILED: %d suite(s) had failures\n", failed);
        printf("ZYNQ_TEST_EXIT:FAIL\n");
    } else {
        printf("ALL SUITES PASSED\n");
        printf("ZYNQ_TEST_EXIT:PASS\n");
    }

    for (;;) {
        /* halt */
    }

    return 0;
}

/*
 * FreeRTOS stubs — linked because osal/claw libs reference them,
 * but the scheduler is never started in test mode.
 */
void vApplicationMallocFailedHook(void) { for (;;); }
void vApplicationStackOverflowHook(void *t, char *n)
{
    (void)t; (void)n; for (;;);
}
void vApplicationIdleHook(void) {}
void vAssertCalled(const char *f, unsigned long l)
{
    (void)f; (void)l; for (;;);
}
void vConfigureTickInterrupt(void) {}
void vClearTickInterrupt(void) {}
void vApplicationFPUSafeIRQHandler(unsigned int u) { (void)u; }
