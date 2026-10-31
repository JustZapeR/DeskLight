#!/usr/bin/env python3
"""Record one actual simulator session. Wall time is only for pacing/watchdogs."""
import argparse, hashlib, json, re, time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--port', type=int, default=42731)
p.add_argument('--seconds', type=int, default=160)
p.add_argument('--min-exports', type=int, default=45)
p.add_argument('--max-mcu-seconds', type=int, default=300)
p.add_argument('--stall-timeout', type=int, default=120)
p.add_argument('--wall-timeout', type=int, default=1800)
a = p.parse_args()
if min(a.seconds, a.min_exports, a.max_mcu_seconds, a.stall_timeout, a.wall_timeout) <= 0:
    p.error('all limits must be positive')
if a.max_mcu_seconds < a.seconds: p.error('maximum MCU duration must cover target duration')
a.output.mkdir(parents=True, exist_ok=False)
root = Path(__file__).resolve().parent
files = ['firmware.elf', 'firmware.hex', 'diagram.json', 'wokwi.toml', 'trigger.chip.c', 'trigger.chip.json', 'trigger.chip.wasm', 'record.py']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
meta = {'scenario':'S3', 'completed':False, 'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
        'fingerprints':{f:sha(root/f) for f in files}, 'first_mcu_ms':None, 'last_mcu_ms':None,
        'full_exports':0, 'partial_exports':0, 'target_span_s':a.seconds, 'target_full_exports':a.min_exports,
        'max_mcu_seconds':a.max_mcu_seconds, 'gate_passed':False, 'reason':'interrupted or connection failed'}
manifest = a.output/'manifest.json'
def save(): manifest.write_text(json.dumps(meta, indent=2)+'\n')
save()
start = time.monotonic(); progress = start; pending = b''; first = None; latest = None
last_history = None; full = 0; partial = 0; in_flight = False
build = False; samples = {}; stats_requested = False; stress_started = False
port = None
try:
    port = serial.serial_for_url(f'rfc2217://localhost:{a.port}', baudrate=115200, timeout=.05)
    with (a.output/'uart.raw').open('wb') as raw, (a.output/'commands.jsonl').open('w') as commands:
        def send(command):
            port.write((command+'\n').encode('ascii'))
            commands.write(json.dumps({'wall_s':round(time.monotonic()-start,3),'last_status_ms':latest,'command':command})+'\n'); commands.flush()
        while time.monotonic()-start < a.wall_timeout:
            if time.monotonic()-progress >= a.stall_timeout:
                meta['reason']='STATUS clock stalled or absent'; break
            data = port.read(4096)
            if data:
                raw.write(data); raw.flush(); pending += data
                while b'\n' in pending:
                    line, pending = pending.split(b'\n',1)
                    m = re.search(rb'STATUS t=(\d+)', line)
                    if m:
                        t = int(m[1])
                        if latest is not None and t < latest:
                            meta['reason']=f'STATUS clock moved backwards {latest} -> {t}'; raise RuntimeError(meta['reason'])
                        if t != latest: progress = time.monotonic()
                        latest = t
                        if first is None: first = t
                    if b'BUILD base=2b6659f7746d tree=c26a59c460911c2e691ed9ab4335fe250ac6bde2b2efe9431ac8a830be848395' in line:
                        build = True
                    m = re.search(rb'STAT name=(adc|imu) n=(\d+)', line)
                    if m: samples[m[1].decode()] = int(m[2])
                    m = re.search(rb'EXPORT t_us=\d+ rows=(\d+) bytes=\d+ elapsed_us=\d+ end_us=\d+',line)
                    if m:
                        if in_flight:
                            if int(m[1]) == 64: full += 1
                            else: partial += 1
                            in_flight = False
                        else:
                            meta['reason']='unexpected EXPORT without recorder request'; raise RuntimeError(meta['reason'])
            if latest is None: continue
            elapsed = (latest-first)/1000
            if elapsed >= a.max_mcu_seconds:
                meta['reason']='maximum MCU span reached before targets'; break
            if elapsed >= 10 and not stats_requested:
                send('stats'); stats_requested = True
            if not meta['gate_passed'] and elapsed >= 10 and build and samples.get('adc',0)>0 and samples.get('imu',0)>0:
                meta['gate_passed'] = True
                meta['gate']={'last_status_ms':latest, 'adc_samples':samples['adc'], 'imu_samples':samples['imu']}
                send('metrics reset'); stress_started = True
            if meta['gate_passed'] and latest >= 70000 and not in_flight and (last_history is None or time.monotonic()-last_history >= .4):
                if elapsed >= a.seconds and full >= a.min_exports:
                    send('stats'); meta['reason']='targets reached'; meta['completed']=True
                    until=time.monotonic()+3
                    while time.monotonic()<until:
                        data=port.read(4096)
                        if data: raw.write(data); raw.flush()
                    break
                send('mode auto'); send('history'); in_flight=True; last_history=time.monotonic()
            meta.update(first_mcu_ms=first,last_mcu_ms=latest,full_exports=full,partial_exports=partial)
            save()
        else: meta['reason']='wall timeout'
except Exception as exc:
    if meta['reason']=='interrupted or connection failed': meta['reason']=repr(exc)
finally:
    if port is not None: port.close()
    meta.update(first_mcu_ms=first,last_mcu_ms=latest,full_exports=full,partial_exports=partial,
                unfinished_history=in_flight,wall_s=round(time.monotonic()-start,3),
                finished_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()))
    if (a.output/'uart.raw').exists(): meta['uart_sha256']=sha(a.output/'uart.raw')
    save()
print(json.dumps(meta,indent=2))
if not meta['completed']: raise SystemExit(1)
