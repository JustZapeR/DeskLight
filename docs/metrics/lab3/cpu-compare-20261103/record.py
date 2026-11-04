import serial,time,json,re,hashlib
from pathlib import Path
base=Path(__file__).resolve().parent;out=base/'run-01';out.mkdir(exist_ok=False)
s=serial.serial_for_url('rfc2217://127.0.0.1:42745',baudrate=115200,timeout=.05);latest=None;buf=b'';started=time.monotonic();next_s2=22000;next_s3=70000;sent=[];done=False
try:
 with (out/'uart.raw').open('wb') as raw:
  while time.monotonic()-started<240:
   b=s.read(4096)
   if b:
    raw.write(b);raw.flush();buf+=b
    while b'\n' in buf:
     l,buf=buf.split(b'\n',1);m=re.search(rb'STATUS t=(\d+)',l)
     if m:latest=int(m[1])
   if latest is None:continue
   if latest>=next_s2 and next_s2<=37000:
    cmd=('mode adjust','mode auto','journal')[(next_s2//3000)%3];s.write((cmd+'\n').encode());sent.append({'status_ms':latest,'command':cmd});next_s2+=3000
   if latest>=next_s3 and next_s3<=90000:
    s.write(b'history\n');sent.append({'status_ms':latest,'command':'history'});next_s3+=1500
   if latest>=97000:done=True;break
finally:
 s.close();(out/'commands.json').write_text(json.dumps(sent,indent=2));(out/'manifest.json').write_text(json.dumps({'complete':done,'latest_status_ms':latest,'firmware_sha256':hashlib.sha256((base/'sim/firmware.elf').read_bytes()).hexdigest(),'diagram_sha256':hashlib.sha256((base/'sim/diagram.json').read_bytes()).hexdigest(),'stim_sha256':hashlib.sha256((base/'sim/stim.chip.wasm').read_bytes()).hexdigest(),'wall_s':time.monotonic()-started},indent=2))
