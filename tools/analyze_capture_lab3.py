#!/usr/bin/env python3
"""Measure the current DeskLight probes from an actual VCD. No waveform generation."""
import argparse,hashlib,json,re,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('vcd',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
channels={};signals={};events=[];scale=None;current=0;header=True
with a.vcd.open() as f:
 for line in f:
  if '$timescale' in line:
   v=line
   while '$end' not in v:v+=next(f)
   m=re.search(r'(\d+)\s*(ns|us|ms|ps|s)',v)
   scale=int(m[1])*{'ps':1e-6,'ns':1e-3,'us':1,'ms':1000,'s':1e6}[m[2]]
  if line.startswith('$var'):
   fields=line.split();channels[fields[3]]=fields[4];signals[fields[4]]=[]
  if '$enddefinitions' in line:header=False
  if header:continue
  if line.startswith('#'):current=int(line[1:]);continue
  m=re.match(r'([01])(.+?)\s*$',line)
  if m and m[2] in channels:signals[channels[m[2]]].append((current,int(m[1])))
if scale is None:raise SystemExit('No timescale; refusing to assume nanoseconds')
def dist(x):
 if not x:return {'n':0,'status':'not measured'}
 return {'n':len(x),'min_us':min(x),'avg_us':statistics.mean(x),'max_us':max(x)}
def pulses(name):
 result=[];start=None
 for t,level in signals.get(name,[]):
  if level==1:start=t
  elif start is not None:result.append((start*scale,t*scale));start=None
 return result
report={'source_sha256':hashlib.sha256(a.vcd.read_bytes()).hexdigest(),'duration_us':current*scale,'channels':channels,'probes':{}}
for name in ['D0','D1','D4']:
 ps=pulses(name);starts=[x[0] for x in ps]
 report['probes'][name]={'width':dist([b-a for a,b in ps]),'period':dist([b-a for a,b in zip(starts,starts[1:])])}
# D7 toggles for each handled rising PIR, so every D7 transition is a response.
rises=[t*scale for t,v in signals.get('D3',[]) if v==1 and t>0]
reactions=[t*scale for t,v in signals.get('D7',[]) if t>0]
lat=[];last_response=-1;ambiguous=0;pairs=[]
for response in reactions:
 pending=[r for r in rises if last_response<r<=response]
 if len(pending)==1:
  lat.append(response-pending[0]);pairs.append({'edge_us':pending[0],'response_us':response,'latency_us':response-pending[0]})
 elif len(pending)>1:ambiguous+=1
 last_response=response
report['pairs']=pairs;report['ambiguous_pairs']=ambiguous
report['pir_to_marker']=dist(lat)
report['pir_during_long_operation']=sum(any(a<=r<=b for a,b in pulses('D1')) for r in rises)
report['limits']='VCD timestamps in simulator time; D1 includes export or real flash calls; check raw records for truncated buffer and startup initial levels. PIR marker is handled poll, not necessarily LED.'
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
