# SPDX-License-Identifier: MIT
# Signal handling tests for Linux native platform.

import signal
import subprocess
import time

from rtclaw_test import RTClawLinuxTest


class TestLinuxSignal(RTClawLinuxTest):
    """Test process signal handling and graceful shutdown."""

    def test_sigterm_graceful_exit(self):
        """SIGTERM must cause a graceful exit with rc=0."""
        self.wait_for_shell_prompt()

        self._proc.send_signal(signal.SIGTERM)
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.fail("Process did not exit after SIGTERM")

        output = self.console.get_output()
        self.assertIn("Bye!", output)
        self.assertEqual(self._proc.returncode, 0)

    def test_sigint_graceful_exit(self):
        """SIGINT must cause a graceful exit with rc=0."""
        self.wait_for_shell_prompt()

        self._proc.send_signal(signal.SIGINT)
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.fail("Process did not exit after SIGINT")

        output = self.console.get_output()
        self.assertIn("Bye!", output)
        self.assertEqual(self._proc.returncode, 0)

    def test_stdin_eof_exit(self):
        """Closing stdin (EOF) must cause the process to exit."""
        self.wait_for_shell_prompt()

        self._proc.stdin.close()
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.fail("Process did not exit after stdin EOF")

        output = self.console.get_output()
        self.assertIn("Bye!", output)
