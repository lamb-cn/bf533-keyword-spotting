"""Prepare one offline target fixture and reproducible packed model image."""
from pathlib import Path
import argparse,re,json,hashlib
import numpy as np
P=Path(__file__).resolve().parent
raw=(P/'weights.h').read_text()
W={n:np.array([int(v) for v in re.findall(r'-?\d+',a)],dtype=np.int32)
   for n,a in re.findall(r'#define\s+(\w+)\s*\{([^}]+)\}',raw)}
np.savez(P/'weights.npz',**W)
import reference as r
parser=argparse.ArgumentParser()
parser.add_argument('--audio',type=Path)
args=parser.parse_args()
file=args.audio or sorted((r.ROOT/'data/speech_commands_test_small/go').glob('*.wav'))[0]
if not file.is_absolute():file=r.ROOT/file
pcm=r.read_audio(file)
image=bytearray(65536);image[:32000]=pcm.astype('<i2').tobytes()
meta=[]
configs=[('CONV1',0,2,6),('CONV2_DS',1,2,5),('CONV2_PW',2,4,8),
 ('CONV3_DS',1,2,4),('CONV3_PW',2,5,8),('CONV4_DS',1,3,5),
 ('CONV4_PW',2,5,7),('CONV5_DS',1,3,5),('CONV5_PW',2,5,8),('FINAL_FC',3,2,7)]
for name,kind,bl,rs in configs:
 w=W[name+'_WT']
 if kind in (0,2,3):
  cols=40 if kind==0 else 64
  w=w.reshape(-1,2,cols).transpose(0,2,1).reshape(-1)
 address=0x01000000+len(image);data=w.astype('<i2').tobytes();image+=data
 meta.append(dict(name=name,kind=kind,bl=bl,rs=rs,address=address,size=len(data),bias=W[name+'_BIAS'].tolist()))
(P/'image.bin').write_bytes(image)
source='typedef struct {unsigned address,bytes;int kind,bl,rs;signed char bias[64];} Layer;\nstatic const Layer layers[10]={\n'
for m in meta:
 source+='{0x%x,%d,%d,%d,%d,{%s}},\n'%(m['address'],m['size'],m['kind'],m['bl'],m['rs'],','.join(map(str,m['bias'])))
(P/'model.h').write_text(source+'};\n')
sha=lambda b:hashlib.sha256(b).hexdigest()
(P/'target_audio.json').write_text(json.dumps(dict(file=str(file.relative_to(r.ROOT)),audio_sha256=sha(file.read_bytes()),weights_sha256=sha((P/'weights.h').read_bytes()),image_sha256=sha(image),layers=meta),indent=2),encoding='utf-8')
print('Prepared',file,'bytes',len(image))
