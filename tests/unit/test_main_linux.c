/* SPDX-License-Identifier: MIT */
/*
 * Unit test entry for Linux native platform.
 * Sets HOME to a temporary directory so KV tests don't
 * pollute the real home directory or read stale data.
 */

#include "test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char tmpdir[] = "/tmp/rtclaw-test-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        perror("mkdtemp");
        return 1;
    }
    setenv("HOME", tmpdir, 1);

    int rc = run_all_unit_tests();

    /* Clean up temp KV files */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    return rc;
}
