"""Exercise the shipped Python AES-GCM sender against the native receiver."""

from __future__ import annotations

import json
from pathlib import Path
import socket
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from openseeface_adapter import SF2Sender
from send_demo import demo_values


def main(executable: str) -> None:
    token = "0123456789abcdef0123456789abcdef"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    process = subprocess.Popen(
        [executable, "--port", str(port), "--token", token, "--duration", "2.5"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    sender = None
    try:
        time.sleep(0.2)
        sender = SF2Sender("127.0.0.1", port, token)
        for _ in range(25):
            sender.send(demo_values(1.0))
            time.sleep(0.05)
        output, errors = process.communicate(timeout=5)
        if process.returncode != 0:
            raise AssertionError(f"receiver failed: {errors}")
        states = [json.loads(line) for line in output.splitlines() if line.startswith("{")]
        if not states or max(state["accepted"] for state in states) < 3:
            raise AssertionError(f"Python sender did not reach C++ receiver: {output} {errors}")
        print("Python and C++ AES-GCM wire interoperability passed")
    finally:
        if sender:
            sender.close()
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)


if __name__ == "__main__":
    main(sys.argv[1])
