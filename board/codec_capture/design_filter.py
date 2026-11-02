"""Reproduce quantized anti-alias coefficients and verify numerical response."""
from pathlib import Path
import json,numpy as np
p=Path(__file__).parent
n=np.arange(63)-31
h=np.sinc(2*6000/48000*n)*(2*6000/48000)*np.kaiser(63,8);h/=h.sum()
q=np.rint(h*32768).astype(int);q[31]+=32768-int(q.sum())
f=np.linspace(0,24000,131073)
db=20*np.log10(np.maximum(np.abs(np.fft.rfft(q/32768,n=262144)),1e-15))
stop=float(db[f>=8000].max());assert stop< -60
assert np.array_equal(q,q[::-1]) and q.sum()==32768
(p/'decimator_coeffs.h').write_text('/* 48kHz ->16kHz,63 taps,Kaiser beta8,cutoff6kHz,Q15,sum32768. */\nstatic const short decimator_h[63]={'+','.join(map(str,q))+'};\n')
(p/'filter_design.json').write_text(json.dumps(dict(input_rate=48000,output_rate=16000,taps=63,cutoff_hz=6000,kaiser_beta=8,coefficient_sum=int(q.sum()),stopband_from_hz=8000,quantized_max_stopband_db=stop,passband_to_hz=4000,passband_min_db=float(db[f<=4000].min())),indent=2))
print('PASS symmetric Q15 FIR,sum32768,stopband %.2f dB'%stop)
