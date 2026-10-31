#!/usr/bin/env python3
"""Choose a documented measurement fixture only while the simulator is stopped."""
import argparse,json,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('fixture',choices=['product','periodic','periodic-no-uart-probes']);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
if a.fixture=='periodic-no-uart-probes':
    diagram=json.loads((root/'tools/scenarios/periodic-diagram.json').read_text())
    diagram['connections']=[wire for wire in diagram['connections'] if wire[0] not in {'logic:D2','logic:D6'} and wire[1] not in {'logic:D2','logic:D6'}]
    (root/'diagram.json').write_text(json.dumps(diagram,ensure_ascii=False,indent=2)+'\n')
else:
    source=root/'tools/scenarios'/f'{a.fixture}-diagram.json'
    shutil.copyfile(source,root/'diagram.json')
print('Selected',a.fixture,'diagram. periodic is a synthetic digital input replacing the PIR OUT wire, not a physical PIR experiment.')
