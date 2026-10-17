#!/usr/bin/env python3
"""Check the driver's real byte fixture against its immutable VCD excerpt."""
import hashlib
import json
import pathlib
import struct
from inspect_capture import load, i2c

root = pathlib.Path(__file__).resolve().parents[1]
fixture = json.loads((root / "tests/fixtures/mpu-captured.json").read_text())
clip = root / fixture["clip"]
assert hashlib.sha256(clip.read_bytes()).hexdigest() == fixture["clip_sha256"]
header, channels, events, duration = load(clip)
assert fixture["source_sha256"] in header
transactions = i2c(channels, events, duration)
transfer = next(t for t in transactions if t["start_ns"] + fixture["clip_offset_ns"] == fixture["start_ns"])
assert transfer["bytes"][0]["value"] == 0xd1
assert [b["value"] for b in transfer["bytes"][1:]] == fixture["bytes"]
assert all(b["ack"] for b in transfer["bytes"][:-1]) and not transfer["bytes"][-1]["ack"]
assert list(struct.unpack(">7h", bytes(fixture["bytes"]))) == fixture["signed_counts"]
print("PASS captured fixture: VCD SHA256, timestamp, 14 bytes, signed counts, final master NACK")
