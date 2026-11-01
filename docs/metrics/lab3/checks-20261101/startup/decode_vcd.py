from bisect import bisect_right
from pathlib import Path
import json
T={};vals={};clock=0
for l in Path('wokwi.vcd').open():
 l=l.strip()
 if l.startswith('#'):clock=int(l[1:])
 elif len(l)==2 and l[0] in '01' and l[1] in "#'$":
  T.setdefault(l[1],[]).append((clock,int(l[0])))
tx=T['#']; times=[t for t,v in tx]; values=[v for t,v in tx];bit=1e9/115200; i=0;chars=[]
while i<len(tx):
 t,v=tx[i]
 if v==0 and i and values[i-1]==1:
  value=0
  for k in range(8):
   j=bisect_right(times,t+(1.5+k)*bit)-1
   value|=values[j]<<k
  j=bisect_right(times,t+9.5*bit)-1
  if values[j]==1:chars.append((t,value))
  i=bisect_right(times,t+9.8*bit)
 else:i+=1
lines=[];s='';t0=None
for t,v in chars:
 if t0 is None:t0=t
 if v==10:
  lines.append((round(t0/1e9,6),s));s='';t0=None
 elif v!=13:s+=chr(v) if 32<=v<127 else '?'
Path('uart-from-vcd.txt').write_text('\n'.join(f'{t:.6f} {s}' for t,s in lines)+'\n')
print('READY transitions',[(t/1e9,v) for t,v in T.get("'",[])])
print('lines',len(lines));print('\n'.join(f'{t:.6f} {s}' for t,s in lines[:20])); print('capture_end_s',clock/1e9)
