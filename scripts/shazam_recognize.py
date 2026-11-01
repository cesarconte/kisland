#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cesar
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recognise a song from an audio file using the Shazam API (shazamio).

Prints the raw Shazam JSON on stdout (same shape as `songrec audio-file-to-recognized-song`).
"""
import asyncio
import json
import sys


async def main(path: str) -> int:
    from shazamio import Shazam

    shazam = Shazam()
    result = await shazam.recognize(path)
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: shazam_recognize.py <audio-file>", file=sys.stderr)
        sys.exit(2)
    try:
        sys.exit(asyncio.run(main(sys.argv[1])))
    except Exception as exc:  # network down, ffmpeg missing, ...
        print(json.dumps({"error": str(exc)}))
        sys.exit(1)
