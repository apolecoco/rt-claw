# SPDX-License-Identifier: MIT
# Boot and banner tests for Linux native platform.

from rtclaw_test import RTClawLinuxTest


class TestLinuxBoot(RTClawLinuxTest):
    """Verify the Linux native binary boots and shows the banner."""

    def test_banner(self):
        """Boot banner must contain 'Real-Time Claw'."""
        self.wait_for_boot()
        output = self.console.get_output()
        self.assertIn("Real-Time Claw", output)

    def test_version_banner(self):
        """Boot banner must contain 'rt-claw v'."""
        self.wait_for_boot()
        output = self.console.get_output()
        self.assertIn("rt-claw v", output)

    def test_shell_prompt(self):
        """Shell prompt must appear after boot."""
        self.wait_for_shell_prompt()
