# SPDX-License-Identifier: MIT
# Console buffer and interaction functions for QEMU subprocess I/O.

import re
import threading
import time

_ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def _strip_ansi(text: str) -> str:
    return _ANSI_RE.sub("", text)


class ConsoleBuffer:
    """
    Wraps a subprocess.Popen with background stdout reader.

    A background thread continuously reads lines from QEMU's stdout
    and appends them to an internal buffer. The main thread polls
    this buffer for pattern matching.
    """

    def __init__(self, proc):
        self.proc = proc
        self._lines = []
        self._lock = threading.Lock()
        self._closed = False
        self._reader = threading.Thread(
            target=self._read_loop, daemon=True
        )
        self._reader.start()

    def _read_loop(self):
        try:
            for raw in self.proc.stdout:
                line = _strip_ansi(raw.decode("utf-8", errors="replace"))
                with self._lock:
                    self._lines.append(line)
        except (ValueError, OSError):
            pass
        finally:
            with self._lock:
                self._closed = True

    def get_lines(self):
        with self._lock:
            return list(self._lines)

    def get_output(self):
        return "".join(self.get_lines())

    @property
    def is_alive(self):
        return self.proc.poll() is None

    def send(self, text: str):
        if self.proc.stdin:
            self.proc.stdin.write(text.encode("utf-8"))
            self.proc.stdin.flush()


def wait_for_console_pattern(console: ConsoleBuffer,
                             pattern: str,
                             timeout: float = 60.0) -> str:
    """
    Wait until `pattern` appears in console output.

    Returns the full output up to (and including) the matching line.
    Raises TimeoutError if not found within timeout seconds.
    Raises RuntimeError if QEMU exits before pattern is found.
    """
    deadline = time.monotonic() + timeout
    seen = 0

    while True:
        lines = console.get_lines()

        for i in range(seen, len(lines)):
            if pattern in lines[i]:
                return "".join(lines[:i + 1])
        seen = len(lines)

        if time.monotonic() >= deadline:
            output = console.get_output()
            raise TimeoutError(
                f"Pattern {pattern!r} not found within {timeout}s.\n"
                f"--- Console output ({len(lines)} lines) ---\n"
                f"{output[-2000:]}"
            )

        if not console.is_alive:
            output = console.get_output()
            raise RuntimeError(
                f"QEMU exited (rc={console.proc.returncode}) "
                f"before pattern {pattern!r} was found.\n"
                f"--- Console output ---\n"
                f"{output[-2000:]}"
            )

        time.sleep(0.1)


def exec_command(console: ConsoleBuffer, command: str):
    """Send a command string followed by newline."""
    console.send(command + "\n")


def exec_command_and_wait_for_pattern(console: ConsoleBuffer,
                                      command: str,
                                      pattern: str,
                                      timeout: float = 30.0) -> str:
    """Send a command and wait for a pattern in the output."""
    exec_command(console, command)
    return wait_for_console_pattern(console, pattern, timeout)
