#!/usr/bin/env python3
"""Read MCU-stamped evidence, without fabricating missing scenarios."""
import argparse,json,re,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('uart',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
text=a.uart.read_bytes().decode('ascii','replace')
rows=[]
for line in text.splitlines():
 if line.startswith(('STAT ','LOAD ','SAVE ','JOURNAL ','EVENT ','COMMAND ','EXPORT ')):
  rows.append({'type':line.split()[0],**dict(re.findall(r'(\w+)=([^\s]+)',line))})
def distribution(values):
 if not values:return {'n':0,'status':'not measured'}
 s=sorted(values)
 return {'n':len(s),'min':min(s),'avg':statistics.mean(s),'max':max(s),'p99':s[min(len(s)-1,(99*len(s)+99)//100-1)]}
events=[int(r['delay_us']) for r in rows if r['type']=='EVENT']
commands=[(int(r['enqueue_done_us'])-int(r['received_us']))&0xffffffff for r in rows if r['type']=='COMMAND']
exports=[int(r['elapsed_us']) for r in rows if r['type']=='EXPORT']
a.output.write_text(json.dumps({'source':str(a.uart),'rows':rows,'event_us':distribution(events),'command_enqueue_us':distribution(commands),'export_us':distribution(exports),'limits':'command enqueue is not wire reply; timestamps are MCU time; stats may be absent after TX drop'},indent=2)+'\n')
print(json.dumps({'event_us':distribution(events),'command_enqueue_us':distribution(commands),'export_us':distribution(exports)}))
