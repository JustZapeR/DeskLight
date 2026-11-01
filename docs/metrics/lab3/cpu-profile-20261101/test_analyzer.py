from pathlib import Path
from tempfile import TemporaryDirectory
from analyze_vcd import analyze
with TemporaryDirectory() as d:
 p=Path(d)/'t.vcd';p.write_text('$timescale 1ns $end\n$var wire 1 ! D0 $end\n$var wire 1 " D1 $end\n$enddefinitions $end\n#0\n0!\n0"\n#1\n1"\n#3\n0"\n#4\n1!\n#10\n0!\n#12\n1!\n#20\n0!\n#30\n')
 r=analyze(p,5e-9,15e-9);assert abs(r['marker_high_ns']-8)<1e-9 and r['marked_rises']==1 and r['calibration_ns_mean']==2,r
 try:analyze(p,0,31e-9);raise AssertionError('incomplete capture accepted')
 except ValueError:pass
print('PASS: clipping, calibration, incomplete capture')
