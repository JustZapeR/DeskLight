#!/usr/bin/env python3
"""Record a single Wokwi instance; host pacing is not MCU timing."""
import hashlib,json,re,serial,time
from pathlib import Path
p=Path(__file__).resolve().parent; out=p/'run-01'; out.mkdir(exist_ok=False)
files=['firmware.elf','firmware.hex','diagram.json','wokwi.toml','fault.chip.c','fault.chip.json','fault.chip.wasm','record.py']; sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
meta={'complete':False,'start_utc':time.strftime('%FT%TZ',time.gmtime()),'files':{x:sha(p/x) for x in files},'reason':'interrupted'}
start=time.monotonic();latest=None;first=None;progress=start;buf=b'';phase='initial'; stage=0; rates=[1,2,5,10,20,50]; sent=0;next_send=0; quiet_until=None;marker=None
(out/'manifest.json').write_text(json.dumps(meta,indent=2))
port=serial.serial_for_url('rfc2217://localhost:42741',baudrate=115200,timeout=.01)
try:
 with (out/'uart.raw').open('wb') as raw,(out/'commands.jsonl').open('w') as commands:
  def send(x):
   port.write((x+'\n').encode());commands.write(json.dumps({'wall_s':round(time.monotonic()-start,6),'mcu_ms':latest,'phase':phase,'command':x})+'\n');commands.flush()
  while time.monotonic()-start<600:
   now=time.monotonic()
   if now-progress>90:meta['reason']='STATUS stalled';break
   data=port.read(4096)
   if data:
    raw.write(data);raw.flush();buf+=data
    while b'\n' in buf:
     line,buf=buf.split(b'\n',1); m=re.search(rb'STATUS t=(\d+)',line)
     if m:
      t=int(m[1]);
      if latest is not None and t<latest: raise RuntimeError('STATUS clock moved backwards')
      if t!=latest:progress=now
      latest=t
      if first is None:first=t
   if latest is None: continue
   if phase=='initial' and latest>=26000:
    phase='sweep';stage=0;marker=latest;send('metrics reset');next_send=now
   if phase=='sweep':
    if latest-marker>=10000:
     phase='drain';quiet_until=now+3
    elif now>=next_send:
     send('mode auto');sent+=1;next_send=now+1/rates[stage]
   elif phase=='drain' and now>=quiet_until:
    phase='snapshot';send('stats');quiet_until=now+4
   elif phase=='snapshot' and now>=quiet_until:
    stage+=1
    if stage==len(rates):meta['complete']=True;meta['reason']='all levels requested';break
    phase='sweep';marker=latest;send('metrics reset');next_send=now
  else:meta['reason']='wall timeout'
except Exception as e:meta['reason']=str(e)
finally:
 port.close();meta.update(first_mcu_ms=first,last_mcu_ms=latest,commands_sent=sent,wall_s=time.monotonic()-start,finished_utc=time.strftime('%FT%TZ',time.gmtime()));meta['uart_sha256']=sha(out/'uart.raw');(out/'manifest.json').write_text(json.dumps(meta,indent=2))
print(json.dumps(meta));raise SystemExit(0 if meta['complete'] else 1)
