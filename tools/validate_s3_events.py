#!/usr/bin/env python3
"""Cross-check actual input edges against VCD pulses and UART export intervals.

This does not generate test events or count flash calls as exports. Timestamps must
come from the same simulation and the same boot; inspect the manifest and capture
coverage separately. MCU times in the UART are quantized to 1 ms.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('uart', type=Path)
p.add_argument('vcd', type=Path)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--tolerance-us', type=float, default=5000)
a = p.parse_args()
raw = a.uart.read_bytes()
exports = [(int(x), int(y)) for x, y in re.findall(
    rb'EXPORT t_us=(\d+) rows=\d+ bytes=\d+ elapsed_us=\d+ end_us=(\d+)', raw)]
signals = {}
ids = {}
stamp = 0
scale = None
with a.vcd.open() as f:
    for line in f:
        if '$timescale' in line:
            v = line
            while '$end' not in v:
                v += next(f)
            m = re.search(r'(\d+)\s*(ns|us|ms|ps|s)', v)
            if not m:
                raise SystemExit('Invalid VCD timescale')
            scale = int(m[1]) * {'ps': 1e-6, 'ns': 1e-3, 'us': 1, 'ms': 1000, 's': 1e6}[m[2]]
        elif line.startswith('$var '):
            fields = line.split()
            ids[fields[3]] = fields[4]
            if fields[4] in ('D1', 'D3'):
                signals[fields[4]] = []
        elif line.startswith('#'):
            stamp = int(line[1:])
        else:
            m = re.match(r'([01])(.+?)\s*$', line)
            if m and ids.get(m[2]) in signals:
                signals[ids[m[2]]].append((stamp, int(m[1])))
if scale is None or not all(k in signals for k in ('D1', 'D3')):
    raise SystemExit('VCD timescale or required D1/D3 channels missing')
pulses = []
start = None
last_d1 = None
for t, level in signals['D1']:
    if level == last_d1:
        continue
    last_d1 = level
    if level:
        start = t * scale
    elif start is not None:
        pulses.append((start, t * scale))
        start = None
rises = []
last_d3 = None
for t, level in signals['D3']:
    if level == 1 and last_d3 == 0 and t > 0:
        rises.append(t * scale)
    last_d3 = level
matched = []
ambiguous = []
for begin, end in pulses:
    candidates = [(i, x, y) for i, (x, y) in enumerate(exports)
                  if abs(begin - x) <= a.tolerance_us and abs(end - y) <= a.tolerance_us]
    if len(candidates) == 1:
        i, x, y = candidates[0]
        matched.append({'vcd_start_us': begin, 'vcd_end_us': end,
                        'uart_export_index': i, 'uart_start_us': x, 'uart_end_us': y,
                        'input_rises_us': [r for r in rises if begin <= r <= end]})
    elif len(candidates) > 1:
        ambiguous.append({'vcd_start_us': begin, 'vcd_end_us': end, 'candidates': len(candidates)})
counts = [r for m in matched for r in m['input_rises_us']]
report = {'uart_sha256': hashlib.sha256(raw).hexdigest(),
          'vcd_sha256': hashlib.sha256(a.vcd.read_bytes()).hexdigest(),
          'vcd_end_us': stamp * scale, 'tolerance_us': a.tolerance_us,
          'uart_exports': len(exports), 'd1_complete_pulses': len(pulses),
          'matched_exports': len(matched), 'ambiguous_pulses': ambiguous,
          'distinct_rises_during_matched_exports': len(set(counts)),
          'matched': matched,
          'limitations': 'Check same run/build, VCD buffer completeness, initial signal levels and UART loss. D1 alone also marks flash. D3 is a digital stimulus; D7 is a handled-event marker, not necessarily an LED.'}
a.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: report[k] for k in ('vcd_end_us', 'uart_exports', 'd1_complete_pulses', 'matched_exports', 'distinct_rises_during_matched_exports')}))
