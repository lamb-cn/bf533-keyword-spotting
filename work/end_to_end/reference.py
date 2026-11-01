from pathlib import Path
import numpy as np
import wave,json,csv
ROOT=Path(__file__).resolve().parents[2]
P=Path(__file__).parent
W=dict(np.load(P/'weights.npz'))
LABELS=['_silence_','_unknown_','yes','no','up','down','left','right','on','off','stop','go']
def nearest(x):return np.sign(x)*np.floor(np.abs(x)+.5)
mel=lambda f:1127*np.log1p(f/700)
freq=mel(np.arange(512)*16000/1024)
edges=np.linspace(mel(20),mel(4000),42)
B=np.maximum(0,np.minimum((freq[None,:]-edges[:-2,None])/(edges[1:-1,None]-edges[:-2,None]),(edges[2:,None]-freq[None,:])/(edges[2:,None]-edges[1:-1,None])))
D=np.sqrt(2/40)*np.cos(np.pi/40*(np.arange(40)[None,:]+.5)*np.arange(10)[:,None])
def mfcc(pcm):
 frames=np.stack([pcm[i*320:i*320+640] for i in range(49)]).astype(np.float64)/32768
 frames*=.5-.5*np.cos(2*np.pi*np.arange(640)/640)
 magnitude=np.abs(np.fft.rfft(frames,n=1024))[:,:512]
 e=magnitude@B.T
 log=np.log(np.maximum(e,np.finfo(np.float32).tiny))
 return np.clip(nearest((log@D.T)*2),-128,127).astype(np.int8)
def quant(x,b,bl,rs):return np.clip((x+b*(1<<bl)+(1<<(rs-1)))>>rs,0,127).astype(np.int8)
def network(features,trace=False):
 x=features.astype(np.int32).reshape(49,10,1)
 pad=np.pad(x,((4,5),(1,1),(0,0)))
 patches=np.stack([pad[y*2:y*2+10,c*2:c*2+4].reshape(-1) for y in range(25) for c in range(5)])
 x=quant(patches@W['CONV1_WT'].reshape(64,40).T,W['CONV1_BIAS'],2,6).reshape(25,5,64)
 layers=[x.copy()]
 for n,db,dr,pb,pr in [(2,2,5,4,8),(3,2,4,5,8),(4,3,5,5,7),(5,3,5,5,8)]:
  pad=np.pad(x.astype(np.int32),((1,1),(1,1),(0,0)))
  wt=W[f'CONV{n}_DS_WT'].reshape(3,3,64)
  s=sum(pad[y:y+25,c:c+5]*wt[y,c] for y in range(3) for c in range(3))
  x=quant(s,W[f'CONV{n}_DS_BIAS'],db,dr);layers.append(x.copy())
  x=quant(x.astype(np.int32)@W[f'CONV{n}_PW_WT'].reshape(64,64).T,W[f'CONV{n}_PW_BIAS'],pb,pr);layers.append(x.copy())
 pooled=np.trunc(x.astype(np.int32).sum((0,1))*4/125).astype(np.int8)
 scores=np.clip((W['FINAL_FC_WT'].reshape(12,64)@pooled.astype(np.int32)+W['FINAL_FC_BIAS']*4+64)>>7,-128,127).astype(np.int8)
 return scores,layers,pooled
def read_audio(path):
 with wave.open(str(path),'rb') as f:
  assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(1,2,16000)
  x=np.frombuffer(f.readframes(f.getnframes()),'<i2')
 return np.pad(x[:16000],(0,max(0,16000-len(x))))
if __name__=='__main__':
 rows=[]
 for file in sorted((ROOT/'data/speech_commands_test_small').glob('*/*.wav')):
  pcm=read_audio(file);features=mfcc(pcm);scores,layers,pool=network(features)
  # Argmax is host reporting only, not a new target operator.
  predicted=LABELS[int(scores.argmax())]
  rows.append(dict(file=str(file.relative_to(ROOT)),label=file.parent.name,predicted=predicted,correct=predicted==file.parent.name,scores=scores.tolist()))
  if len(rows)==1:
   pcm.astype('<i2').tofile(P/'audio.bin');features.tofile(P/'mfcc_float.bin')
   np.concatenate([a.reshape(-1) for a in layers]+[pool,scores]).tofile(P/'network_golden.bin')
 (P/'pc_results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
 print('PC float-MFCC reference:',sum(r['correct'] for r in rows),'/',len(rows))
 print('First sample:',rows[0])
