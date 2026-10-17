#!/usr/bin/env python3
"""Decode saved DeskLight VCD transitions and retain measured signal times."""
import argparse
import bisect
import hashlib
import itertools
import json
import pathlib
import re
import statistics


def load(path):
    lines = path.read_text().splitlines()
    end = next(i for i, s in enumerate(lines) if "$enddefinitions" in s)
    header = "\n".join(lines[:end + 1]) + "\n"
    scale_match = re.search(r"\$timescale\s+(\d+)\s*(s|ms|us|ns|ps)\s+\$end", header)
    factor = int(scale_match[1]) * {"s": 1e9, "ms": 1e6, "us": 1e3, "ns": 1, "ps": .001}[scale_match[2]]
    channels = {}
    for line in lines[:end]:
        fields = line.split()
        if fields[:1] == ["$var"] and fields[2] == "1":
            channels[fields[3]] = fields[4]
    events = []
    tick = 0
    for line in lines[end + 1:]:
        line = line.strip()
        if line.startswith("#"):
            tick = round(int(line[1:]) * factor)
        elif line and line[0] in "01" and line[1:] in channels:
            events.append((tick, line[1:], int(line[0])))
    return header, channels, events, tick


def i2c(channels, events, duration):
    ids = {v: k for k, v in channels.items()}
    scl, sda = ids["D0"], ids["D1"]
    values = dict.fromkeys(channels, 0)
    transactions = []
    current = None
    bits = []
    for t, grouped in itertools.groupby(events, key=lambda e: e[0]):
        old_clock, old_data = values[scl], values[sda]
        for _, code, value in grouped:
            values[code] = value
        clock, data = values[scl], values[sda]
        if old_clock and clock and old_data != data:
            if data == 0:
                if current:
                    current["end_ns"] = t
                    current["termination"] = "repeated_start"
                    transactions.append(current)
                current = {"start_ns": t, "repeated": current is not None, "bytes": []}
                bits = []
            elif current:
                current["end_ns"] = t
                current["termination"] = "stop"
                transactions.append(current)
                current = None
                bits = []
        elif not old_clock and clock and current is not None:
            bits.append((t, data))
            if len(bits) == 9:
                byte = sum(b << (7 - n) for n, (_, b) in enumerate(bits[:8]))
                current["bytes"].append({"value": byte, "ack": bits[8][1] == 0,
                                         "bits": [b for _, b in bits], "first_bit_ns": bits[0][0]})
                bits = []
    if current:
        current["end_ns"] = duration
        current["termination"] = "capture_end"
        current["trailing_bits"] = [b for _, b in bits]
        transactions.append(current)
    return transactions


def uart(channels, events, channel, duration, baud=115200):
    code = next(k for k, v in channels.items() if v == channel)
    signal = [(t, v) for t, c, v in events if c == code]
    times = [t for t, _ in signal]
    states = [v for _, v in signal]
    decoded = []
    until = -1
    bit = 1e9 / baud
    def level(t):
        at = bisect.bisect_right(times, t) - 1
        return states[at] if at >= 0 else 1
    for i, (t, value) in enumerate(signal):
        if t == 0 or value or (i and not signal[i - 1][1]) or t < until or t + 9.5 * bit > duration:
            continue
        if level(t + 9.5 * bit) != 1:
            continue
        byte = sum(level(t + (1.5 + n) * bit) << n for n in range(8))
        decoded.append({"ns": t, "byte": byte})
        until = t + 10 * bit
    output, line = [], []
    for byte in decoded:
        line.append(byte)
        if byte["byte"] == 10 or (channel == "D6" and byte["byte"] == 13):
            output.append({"start_ns": line[0]["ns"], "end_ns": round(line[-1]["ns"] + 10 * bit),
                           "text": bytes(b["byte"] for b in line).decode("ascii", errors="replace").rstrip()})
            line = []
    return decoded, output


def timing(channels, events, channel):
    code = next(k for k, v in channels.items() if v == channel)
    edges = [(t, v) for t, c, v in events if c == code and t > 0]
    rises = [t for t, v in edges if v == 1]
    periods = [b - a for a, b in zip(rises, rises[1:])]
    widths = [edges[i + 1][0] - t for i, (t, v) in enumerate(edges[:-1])
              if v == 1 and edges[i + 1][1] == 0]
    return {"rises_ns": rises, "periods_ns": periods, "widths_ns": widths,
            "median_period_ns": statistics.median(periods) if periods else None,
            "median_width_ns": statistics.median(widths) if widths else None}


def clip(path, start, stop, output):
    header, channels, events, duration = load(path)
    if not 0 <= start < stop <= duration:
        raise ValueError("clip must lie within the original capture")
    initial = dict.fromkeys(channels, 0)
    for t, code, value in events:
        if t <= start: initial[code] = value
    # Replace only the timescale representation; signal transitions are retained.
    header = re.sub(r"\$timescale\s+.*?\$end", "$timescale 1 ns $end", header, flags=re.S)
    comment = f"$comment source={path.name} sha256={hashlib.sha256(path.read_bytes()).hexdigest()} offset_ns={start} $end\n"
    at = header.index("$enddefinitions")
    header = header[:at] + comment + header[at:]
    data = [header, "#0\n$dumpvars\n", *[f"{v}{k}\n" for k, v in initial.items()], "$end\n"]
    previous = 0
    for t, code, value in events:
        if start < t <= stop:
            if t - start != previous:
                data.append(f"#{t - start}\n"); previous = t - start
            data.append(f"{value}{code}\n")
    data.append(f"#{stop - start}\n")
    output.write_text("".join(data))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--clip", nargs=2, type=float, metavar=("START_SECONDS", "END_SECONDS"))
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    if args.clip:
        assert args.output
        clip(args.input, round(args.clip[0] * 1e9), round(args.clip[1] * 1e9), args.output)
        return
    _, channels, events, duration = load(args.input)
    tx_bytes, tx_lines = uart(channels, events, "D2", duration)
    rx_bytes, rx_lines = uart(channels, events, "D6", duration)
    scl = timing(channels, events, "D0")
    active_periods = [p for p in scl["periods_ns"] if 0 < p < 100000]
    result = {
        "source": args.input.name, "sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "channels": channels, "last_ns": duration, "i2c": i2c(channels, events, duration),
        "uart_tx": tx_lines, "uart_rx": rx_lines,
        "scl_active_period_ns": statistics.median(active_periods) if active_periods else None,
        "adc_mark": timing(channels, events, "D4"),
        "pir": timing(channels, events, "D3"), "led": timing(channels, events, "D5")}
    base = args.output or args.input.with_suffix("")
    base.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    base.with_suffix(".uart.txt").write_text("\n".join(x["text"] for x in tx_lines) + "\n")
    base.with_suffix(".commands.txt").write_text("\n".join(x["text"] for x in rx_lines) + "\n")
    annotations = []
    for transfer in result["i2c"]:
        payload = " ".join(f"{b['value']:02X}:{'ACK' if b['ack'] else 'NACK'}" for b in transfer["bytes"])
        annotations.append(f"{transfer['start_ns']}..{transfer['end_ns']} ns {payload}")
    base.with_suffix(".i2c.txt").write_text("\n".join(annotations) + "\n")
    print(json.dumps({"transactions": len(result["i2c"]), "uart_bytes": len(tx_bytes),
                      "command_bytes": len(rx_bytes), "scl_period_ns": result["scl_active_period_ns"],
                      "adc_period_ns": result["adc_mark"]["median_period_ns"]}, indent=2))


if __name__ == "__main__":
    main()
