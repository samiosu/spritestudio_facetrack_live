#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Send synthetic SF1 motion to SpriteFace; this does not access a camera."""

from __future__ import annotations

import argparse
import math
import sys
import time

from openseeface_adapter import (
    SF1Sender, add_sender_arguments, clamp01, finite_argument, neutral_values,
)


def demo_values(elapsed: float) -> dict:
    """Deterministic pose, talking, smiling, and simultaneous blinking."""
    blink_time = elapsed % 3.0
    openness = clamp01(abs(blink_time - 0.15) / 0.12) if blink_time < 0.3 else 1.0
    return {"tracking": True, "confidence": 1.0,
            "head": {"yaw": 0.4 * math.sin(elapsed * 0.7),
                     "pitch": 0.15 * math.sin(elapsed * 0.5),
                     "roll": 0.15 * math.sin(elapsed * 0.9)},
            "eyes": {"left": openness, "right": openness},
            "mouth": {"open": max(0.0, math.sin(elapsed * 5.0)) * 0.75,
                      "smile": (math.sin(elapsed * 0.4) + 1.0) * 0.3}}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    add_sender_arguments(parser)
    parser.add_argument("--fps", type=finite_argument, default=30,
                        help="synthetic frame rate (default: 30)")
    parser.add_argument("--duration", type=finite_argument, default=0,
                        help="seconds to send, or 0 until Ctrl+C (default: 0)")
    args = parser.parse_args(argv)
    if not 1 <= args.fps <= 240 or args.duration < 0:
        parser.error("fps must be 1..240 and duration must be nonnegative")
    sender = None
    try:
        sender = SF1Sender(args.host, args.port, args.token)
        print(f"Synthetic motion demo -> {args.host}:{args.port}; no camera is used", flush=True)
        start = time.monotonic()
        next_frame = start
        interval = 1.0 / args.fps
        while True:
            now = time.monotonic()
            elapsed = now - start
            if args.duration and elapsed >= args.duration:
                break
            sender.send(demo_values(elapsed))
            next_frame = max(next_frame + interval, now)
            time.sleep(max(0.0, next_frame - time.monotonic()))
        sender.send(neutral_values())
        return 0
    except KeyboardInterrupt:
        if sender is not None:
            sender.send(neutral_values())
        return 0
    except OSError as error:
        print(f"Synthetic motion demo: {error}", file=sys.stderr)
        return 1
    finally:
        if sender is not None:
            sender.close()


if __name__ == "__main__":
    raise SystemExit(main())
