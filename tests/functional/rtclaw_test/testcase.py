# SPDX-License-Identifier: MIT
# Base test classes for rt-claw functional tests (QEMU and Linux native).

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

    @classmethod
    def setUpClass(cls):
        name = os.environ.get("RTCLAW_TEST_PLATFORM", "esp32c3-qemu")
        cls.platform = get_platform(name)

    def setUp(self):
        self._tmpdir = tempfile.mkdtemp(prefix="rtclaw-test-")

        if not os.path.isfile(self.platform.flash_path):
            self.skipTest(
                f"Flash image not found: {self.platform.flash_path} "
                f"(run 'make build-{self.platform.name}' first)"
            )

        ext = os.path.splitext(self.platform.flash_path)[1]
        self._flash_copy = os.path.join(self._tmpdir, f"flash{ext}")
        shutil.copy2(self.platform.flash_path, self._flash_copy)

        self._sd_copy = ""
        sd_src = self.platform.extra_files.get("sd_bin")
        if sd_src:
            self._sd_copy = os.path.join(self._tmpdir, "sd.bin")
            if os.path.isfile(sd_src):
                shutil.copy2(sd_src, self._sd_copy)
            else:
                with open(self._sd_copy, "wb") as f:
                    f.seek(64 * 1024 * 1024 - 1)
                    f.write(b"\x00")

        self.console = self._launch()

    def tearDown(self):
        self._shutdown()
        if hasattr(self, "_outcome"):
            result = self._outcome.result
            if result and (result.failures or result.errors):
                self._dump_log()
        if os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)

    def _launch(self) -> ConsoleBuffer:
        cmd = build_qemu_command(
            self.platform, self._flash_copy, self._sd_copy or None,
        )
        self._proc = subprocess.Popen(
            cmd, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        return ConsoleBuffer(self._proc)

    def _shutdown(self):
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
        """Kill and restart QEMU (for persistence tests)."""
        self._shutdown()
        self.console = self._launch()

    def wait_for_boot(self, timeout: float = 0):
        if timeout <= 0:
            timeout = self.platform.boot_timeout
        wait_for_console_pattern(
            self.console, self.platform.boot_marker, timeout
        )

    def wait_for_shell_prompt(self, timeout: float = 0):
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
        output = self.console.get_output() if hasattr(self, "console") else ""
        if output:
            print(f"\n--- output ({self.platform.name}) ---")
            print(output[-3000:])
            print("--- end ---")


class RTClawLinuxTest(unittest.TestCase):
    """
    Base class for rt-claw Linux native functional tests.

    Uses PlatformConfig("linux") for markers and timeouts.
    Manages the native subprocess lifecycle with isolated KV dir.
    """

    platform: PlatformConfig
    console: ConsoleBuffer
    _proc: subprocess.Popen
    _tmpdir: str

    @classmethod
    def setUpClass(cls):
        cls.platform = get_platform("linux")
        if not os.path.isfile(cls.platform.flash_path):
            raise unittest.SkipTest(
                f"Linux binary not found: {cls.platform.flash_path} "
                f"(run 'make build-linux' first)"
            )

    def setUp(self):
        self._tmpdir = tempfile.mkdtemp(prefix="rtclaw-linux-test-")
        os.makedirs(os.path.join(self._tmpdir, "kv"), exist_ok=True)
        self.console = self._launch()

    def tearDown(self):
        self._shutdown()
        if hasattr(self, "_outcome"):
            result = self._outcome.result
            if result and (result.failures or result.errors):
                self._dump_log()
        if os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)

    def _launch(self) -> ConsoleBuffer:
        env = os.environ.copy()
        env["HOME"] = self._tmpdir
        env["TERM"] = "dumb"
        self._proc = subprocess.Popen(
            [self.platform.flash_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            env=env,
        )
        return ConsoleBuffer(self._proc)

    def _shutdown(self):
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

    def restart(self):
        """Kill and restart (for persistence tests)."""
        self._shutdown()
        self.console = self._launch()

    def wait_for_boot(self, timeout=0):
        if timeout <= 0:
            timeout = self.platform.boot_timeout
        wait_for_console_pattern(
            self.console, self.platform.boot_marker, timeout
        )

    def wait_for_shell_prompt(self, timeout=0):
        if timeout <= 0:
            timeout = self.platform.boot_timeout
        wait_for_console_pattern(
            self.console, self.platform.shell_prompt, timeout
        )

    def _dump_log(self):
        output = (
            self.console.get_output() if hasattr(self, "console") else ""
        )
        if output:
            print("\n--- Linux output ---")
            print(output[-3000:])
            print("--- end ---")
