#!/usr/bin/env python3
"""Integrate GPIO marker high time over a complete VCD window; nanoseconds."""
import argparse,json,re,hashlib
from pathlib import Path

def analyze(path,start,end):
 channels={}; samples={}; t=0;last=-1; unit=None;enddefs=False
 for line in path.open():
  line=line.strip()
  if line.startswith('$timescale'):
   m=re.search(r'(\d+)\s*(ns|ps|us)',line);unit=(int(m[1])*{'ps':.001,'ns':1,'us':1000}[m[2]]) if m else None
  if line.startswith('$var '):
   a=line.split();
   if a[4] in ('D0','D1'):channels[a[3]]=a[4];samples[a[4]]=[]
  if line.startswith('$enddefinitions'):enddefs=True
  if not enddefs:continue
  if line.startswith('#'):t=int(line[1:]);last=t
  elif len(line)>=2 and line[0] in '01' and line[1:] in channels:samples[channels[line[1:]]].append((t,int(line[0])))
 if unit is None or set(samples)!={'D0','D1'}:raise ValueError('Missing timescale or marker channels')
 a=start*1e9/unit;b=end*1e9/unit
 if last<b:raise ValueError(f'Incomplete VCD: end {last*unit/1e9:.6f}s, required {end}s')
 def integrate(pairs,lo,hi):
  value=0;previous=0;total=0;count=0;widths=[];rise=None
  for ts,state in pairs:
   if ts>hi:break
   if value and ts>lo:total+=max(0,min(ts,hi)-max(previous,lo))
   if state==1 and value==0:
    rise=ts
    if lo<=ts<hi:count+=1
   if state==0 and value==1 and rise is not None:widths.append((ts-rise)*unit)
   value=state;previous=ts
  if value:total+=max(0,hi-max(previous,lo))
  return total*unit,count,widths
 high,n,_=integrate(samples['D0'],a,b);_,cn,cw=integrate(samples['D1'],0,b)
 return {'source':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'capture_end_s':last*unit/1e9,'window_s':[start,end],'marker_high_ns':high,'marked_fraction_pct':100*high/((end-start)*1e9),'marked_rises':n,'calibration_pulses':cn,'calibration_ns_min':min(cw) if cw else None,'calibration_ns_mean':sum(cw)/len(cw) if cw else None,'calibration_ns_max':max(cw) if cw else None,'limitations':'foreground marker includes interrupt preemption and instrumentation; excludes idle polls, exception-only execution outside markers, work outside selected scopes. Not physical CPU measurement.'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('vcd',type=Path);p.add_argument('--start',type=float,default=5);p.add_argument('--end',type=float,default=15);a=p.parse_args();print(json.dumps(analyze(a.vcd,a.start,a.end),ensure_ascii=False,indent=2))
