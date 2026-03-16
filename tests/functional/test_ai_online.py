# SPDX-License-Identifier: MIT
# AI connectivity test — requires API key and network access.

import os
import unittest

from rtclaw_test import RTClawQemuTest, wait_for_console_pattern


@unittest.skipUnless(
    os.environ.get("RTCLAW_AI_API_KEY"),
    "AI online test requires RTCLAW_AI_API_KEY environment variable",
)
@unittest.skipUnless(
    os.environ.get("RTCLAW_TEST_PLATFORM", "esp32c3-qemu")
    in ("esp32c3-qemu", "esp32s3-qemu"),
    "AI online test requires ESP32 QEMU platform",
)
class TestAiOnline(RTClawQemuTest):
    """Test AI boot connectivity (requires API key + network)."""

    def test_ai_boot_response(self):
        """AI boot test must produce '[boot] AI>' response."""
        # AI boot test runs automatically after claw_init
        # Wait for either success or failure marker
        try:
            wait_for_console_pattern(
                self.console, "[boot] AI>",
                timeout=self.platform.boot_timeout * 2,
            )
        except TimeoutError:
            output = self.console.get_output()
            if "[boot] AI test failed:" in output:
                self.fail("AI boot test reported failure")
            raise
