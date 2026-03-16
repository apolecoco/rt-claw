# SPDX-License-Identifier: MIT
# Shell command tests (requires a platform with shell support).

import os
import unittest

from rtclaw_test import (
    RTClawQemuTest,
    exec_command_and_wait_for_pattern,
)


@unittest.skipUnless(
    os.environ.get("RTCLAW_TEST_PLATFORM", "esp32c3-qemu")
    in ("esp32c3-qemu", "esp32s3-qemu"),
    "Shell tests require ESP32 QEMU platform",
)
class TestShell(RTClawQemuTest):
    """Test shell commands via QEMU console."""

    def setUp(self):
        super().setUp()
        self.wait_for_shell_prompt()

    def test_help(self):
        """'/help' must list available commands."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/help",
            "Anything else is sent directly to AI.",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("/help", output)
        self.assertIn("/log", output)
        self.assertIn("/remember", output)

    def test_log_off(self):
        """'/log off' must disable log output."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/log off", "Log output: OFF",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("OFF", output)

    def test_log_on(self):
        """'/log on' must enable log output."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/log on", "Log output: ON",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("ON", output)

    def test_log_level(self):
        """'/log level debug' must set log level."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/log level debug", "Log level: debug",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("debug", output)

    def test_history(self):
        """'/history' must show conversation count."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/history", "messages",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("0 messages", output)

    def test_clear(self):
        """'/clear' must clear conversation memory."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/clear", "cleared",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("cleared", output)

    def test_remember_and_list(self):
        """'/remember' saves a memory, '/memories' lists it."""
        exec_command_and_wait_for_pattern(
            self.console, "/remember testkey testvalue",
            "Remembered:",
            timeout=self.platform.shell_timeout,
        )
        output = exec_command_and_wait_for_pattern(
            self.console, "/memories", "testkey",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("testkey", output)

    def test_ai_status(self):
        """'/ai_status' must show Model field."""
        output = exec_command_and_wait_for_pattern(
            self.console, "/ai_status", "Model:",
            timeout=self.platform.shell_timeout,
        )
        self.assertIn("Model:", output)
