# SPDX-License-Identifier: MIT
# Base test class for rt-claw QEMU functional tests.

import os
import shutil
import signal
import subprocess
import tempfile
import unittest

from rtclaw_test.cmd import ConsoleBuffer, wait_for_console_pattern
from rtclaw_test.platform import (
    PlatformConfig,
    build_qemu_command,
    get_platform,
)


class RTClawQemuTest(unittest.TestCase):
    """
    Base class for rt-claw QEMU functional tests.

    Manages the QEMU subprocess lifecycle:
    - setUp: resolve platform, copy flash image, launch QEMU
    - tearDown: terminate QEMU, clean up temp files
    """

    platform: PlatformConfig
    console: ConsoleBuffer
    _proc: subprocess.Popen
    _tmpdir: str
    _flash_copy: str
    _sd_copy: str
    _log_path: str

    @classmethod
    def setUpClass(cls):
        name = os.environ.get("RTCLAW_TEST_PLATFORM", "esp32c3-qemu")
        cls.platform = get_platform(name)

    def setUp(self):
        self._tmpdir = tempfile.mkdtemp(prefix="rtclaw-test-")

        # Copy flash image to temp dir for isolation
        if not os.path.isfile(self.platform.flash_path):
            self.skipTest(
                f"Flash image not found: {self.platform.flash_path} "
                f"(run 'make build-{self.platform.name}' first)"
            )

        ext = os.path.splitext(self.platform.flash_path)[1]
        self._flash_copy = os.path.join(self._tmpdir, f"flash{ext}")
        shutil.copy2(self.platform.flash_path, self._flash_copy)

        # Copy SD card image if needed (vexpress-a9)
        self._sd_copy = ""
        sd_src = self.platform.extra_files.get("sd_bin")
        if sd_src:
            self._sd_copy = os.path.join(self._tmpdir, "sd.bin")
            if os.path.isfile(sd_src):
                shutil.copy2(sd_src, self._sd_copy)
            else:
                # Create empty 64MB SD image
                with open(self._sd_copy, "wb") as f:
                    f.seek(64 * 1024 * 1024 - 1)
                    f.write(b"\x00")

        self._log_path = os.path.join(self._tmpdir, "qemu.log")
        self.console = self.launch_qemu()

    def tearDown(self):
        self.shutdown_qemu()

        # Save log for debugging if test failed
        if hasattr(self, "_outcome"):
            result = self._outcome.result
            if result and (result.failures or result.errors):
                self._dump_log()

        if os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)

    def launch_qemu(self) -> ConsoleBuffer:
        """Start QEMU subprocess and return a ConsoleBuffer."""
        cmd = build_qemu_command(
            self.platform,
            self._flash_copy,
            self._sd_copy or None,
        )

        self._proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

        return ConsoleBuffer(self._proc)

    def shutdown_qemu(self):
        """Terminate QEMU: SIGTERM -> wait(5s) -> SIGKILL -> close pipes."""
        if not hasattr(self, "_proc"):
            return

        if self._proc.poll() is None:
            self._proc.send_signal(signal.SIGTERM)
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait(timeout=3)

        if self._proc.stdin:
            self._proc.stdin.close()
        if self._proc.stdout:
            self._proc.stdout.close()

    def restart_qemu(self):
        """Kill and restart QEMU with the same flash image (for persistence tests)."""
        self.shutdown_qemu()
        self.console = self.launch_qemu()

    def wait_for_boot(self, timeout: float = 0):
        """Wait for the boot banner to appear."""
        if timeout <= 0:
            timeout = self.platform.boot_timeout
        wait_for_console_pattern(
            self.console, self.platform.boot_marker, timeout
        )

    def wait_for_shell_prompt(self, timeout: float = 0):
        """Wait for the shell prompt to appear."""
        if not self.platform.has_shell:
            self.skipTest(
                f"Platform {self.platform.name} has no shell"
            )
        if timeout <= 0:
            timeout = self.platform.boot_timeout
        wait_for_console_pattern(
            self.console, self.platform.shell_prompt, timeout
        )

    def _dump_log(self):
        """Print console output for debugging failed tests."""
        output = self.console.get_output() if hasattr(self, "console") else ""
        if output:
            print(f"\n--- QEMU output ({self.platform.name}) ---")
            # Print last 3000 chars to avoid flooding
            print(output[-3000:])
            print("--- end ---")
