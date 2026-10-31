"""Stamp the actual source tree, independent of commit dates and uncommitted files."""
from pathlib import Path
import hashlib,subprocess
root=Path(__file__).resolve().parents[1]
paths=sorted(p for p in (root/'firmware').rglob('*') if p.is_file() and p.suffix in {'.c','.h','.s','.ld'} and 'build' not in p.parts and p.name!='build_id.h')
h=hashlib.sha256()
for p in paths+[root/'firmware/Makefile']:
 h.update(str(p.relative_to(root)).encode()+b'\0'+p.read_bytes())
commit=subprocess.check_output(['git','rev-parse','--short=12','HEAD'],cwd=root,text=True).strip()
identifier=f'base={commit} tree={h.hexdigest()}'
(root/'firmware/Board/build_id.h').write_text('#define DESKLIGHT_BUILD_ID "'+identifier+'"\n')
print(identifier)
