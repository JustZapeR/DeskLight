#!/usr/bin/env python3
"""Record a real RFC2217 MCU session. Host times are provenance, never MCU metrics.
Requires pyserial 3.5. Start Wokwi separately with the configured port.
"""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
import serial
p=argparse.ArgumentParser()
p.add_argument('scenario',choices=['S1','S2','S3','fault','boundary'])
p.add_argument('--seconds',type=int,default=300)
p.add_argument('--port',type=int,default=42731)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--command-rate',type=float,default=10)
p.add_argument('--wall-timeout',type=int,default=10800)
p.add_argument('--stall-timeout',type=int,default=30,help='stop as incomplete if MCU STATUS time does not advance for this many wall seconds')
a=p.parse_args()
if a.stall_timeout<=0: p.error('--stall-timeout must be positive')
if a.command_rate<=0: p.error('--command-rate must be positive')
a.output.mkdir(parents=True,exist_ok=False)
root=Path(__file__).resolve().parents[1]
files=[root/'firmware/build/firmware.elf',root/'firmware/build/firmware.hex',root/'firmware/build/firmware.bin',root/'diagram.json',root/'wokwi.toml',Path(__file__)]
meta={'scenario':a.scenario,'requested_mcu_seconds':a.seconds,'port':a.port,
      'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
      'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
      'build_id':(root/'firmware/Board/build_id.h').read_text(),
      'sha256':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in files},
      'clock_policy':'STATUS t MCU milliseconds controls stop; host monotonic used for transport pacing only',
      'completed':False}
(a.output/'manifest.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2))
port=serial.serial_for_url(f'rfc2217://localhost:{a.port}',baudrate=115200,timeout=.005)
start=time.monotonic(); last_progress=start; pending=bytearray(); first=None; latest=0
stats_next=0; command_next=0; export_next=0; fault_stage=0; stress_started=False
s2_stage=0; s2_cycle=300
with (a.output/'uart.raw').open('wb') as raw,(a.output/'commands.jsonl').open('w') as commands:
 def send(text):
  port.write((text+'\n').encode('ascii'))
  commands.write(json.dumps({'host_monotonic_s':time.monotonic()-start,'last_mcu_ms':latest,'command':text})+'\n');commands.flush()
 send('metrics reset')
 while time.monotonic()-start<a.wall_timeout:
  if time.monotonic()-last_progress>=a.stall_timeout:
   meta['incomplete_reason']='MCU STATUS time did not advance before stall timeout'
   break
  data=port.read(4096)
  if data:
   raw.write(data);raw.flush();pending.extend(data)
   while b'\n' in pending:
    line,_,pending=pending.partition(b'\n')
    m=re.search(rb'STATUS t=(\d+)',line)
    if m:
     observed=int(m[1])
     if first is not None and observed < latest:
      meta['incomplete_reason']=f'MCU STATUS time moved backwards: {latest} -> {observed} ms'
      break
     if first is None or observed!=latest: last_progress=time.monotonic()
     latest=observed;first=latest if first is None else first
  if 'incomplete_reason' in meta: break
  if first is None: continue
  elapsed=(latest-first)/1000
  if elapsed>=a.seconds:
   send('stats');send('journal');
   # Read the final stats without assigning host times to MCU events.
   until=time.monotonic()+4
   while time.monotonic()<until:
    data=port.read(4096);raw.write(data)
   meta['completed']=True;break
  if elapsed>=stats_next:
   send('stats');stats_next+=60
  if a.scenario=='S2' and elapsed>=command_next:
   send('journal');command_next+=120
  if a.scenario=='S2':
   schedule=[(s2_cycle,'mode adjust'),(s2_cycle+5,'zero'),(s2_cycle+10,'mode auto')]
   if elapsed>=schedule[s2_stage][0]:
    send(schedule[s2_stage][1]);s2_stage+=1
    if s2_stage==len(schedule): s2_stage=0;s2_cycle+=300
  if a.scenario in {'S3','boundary'} and elapsed>=65 and not stress_started:
   send('metrics reset');stress_started=True;stats_next=elapsed+60
  if a.scenario=='S3' and elapsed>=65:
   wall=time.monotonic()-start
   if wall>=export_next:
    send('history');export_next=wall+1.3
   if wall>=command_next:
    send('mode auto');command_next=wall+1/a.command_rate
  if a.scenario=='boundary' and elapsed>=65:
   wall=time.monotonic()-start
   if wall>=command_next:
    send('history' if int(wall*a.command_rate)%10==0 else 'mode auto')
    command_next=wall+1/a.command_rate
  if a.scenario=='fault':
   schedule=[(2,'storage probe'),(4,'journal'),(5,'fault imu on'),(8,'fault imu off'),(12,'pause imu on'),(14,'pause imu off'),(18,'probe nack'),(22,'save'),(25,'journal')]
   if fault_stage<len(schedule) and elapsed>=schedule[fault_stage][0]:
    send(schedule[fault_stage][1]);fault_stage+=1
port.close()
meta.update({'finished_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'first_mcu_ms':first,'last_mcu_ms':latest,
             'wall_seconds':time.monotonic()-start,'uart_sha256':hashlib.sha256((a.output/'uart.raw').read_bytes()).hexdigest()})
(a.output/'manifest.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2))
print(json.dumps({k:meta[k] for k in ['completed','first_mcu_ms','last_mcu_ms','wall_seconds']}))
if not meta['completed']:raise SystemExit('MCU duration not reached; incomplete evidence')
