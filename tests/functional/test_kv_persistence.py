# SPDX-License-Identifier: MIT
# KV persistence tests — verify data survives QEMU reboot.

import os
import unittest

from rtclaw_test import (
    RTClawQemuTest,
    exec_command_and_wait_for_pattern,
)


@unittest.skipUnless(
    os.environ.get("RTCLAW_TEST_PLATFORM", "esp32c3-qemu")
    in ("esp32c3-qemu", "esp32s3-qemu"),
    "KV persistence tests require ESP32 QEMU platform",
)
class TestKvPersistence(RTClawQemuTest):
    """Test that long-term memory persists across QEMU reboots."""

    def test_ltm_survives_reboot(self):
        """'/remember' data must survive kill + restart."""
        self.wait_for_shell_prompt()

        # Save a memory
        exec_command_and_wait_for_pattern(
            self.console, "/remember persist_test reboot_value",
            "Remembered:",
            timeout=self.platform.shell_timeout,
        )

        # Kill and restart QEMU (same flash image)
        self.restart_qemu()
        self.wait_for_shell_prompt()

        # Verify memory persists
        output = exec_command_and_wait_for_pattern(
            self.console, "/memories", "persist_test",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("persist_test", output)
        self.assertIn("reboot_value", output)
