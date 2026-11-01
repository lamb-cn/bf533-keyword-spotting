from pathlib import Path
import sys,json,wave,hashlib
import numpy as np
P=Path(__file__).resolve().parent;END=P.parent/'end_to_end'
sys.path.insert(0,str(END));import reference as r
sequence=['go','left','go','go','right','on','off','stop','yes','no','up','down']
parts=[np.zeros(16000,dtype=np.int16)];events=[];position=16000
for label in sequence:
 path=sorted((r.ROOT/'data/speech_commands_test_small'/label).glob('*.wav'))[0]
 pcm=r.read_audio(path)
 events.append(dict(label=label,file=str(path.relative_to(r.ROOT)),start_sample=position,end_sample=position+16000,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
 parts.extend([pcm,np.zeros(10400,dtype=np.int16)]);position+=26400
parts.append(np.zeros(16000,dtype=np.int16));pcm=np.concatenate(parts)
with wave.open(str(P/'long_keywords.wav'),'wb') as f:
 f.setparams((1,2,16000,0,'NONE',''));f.writeframes(pcm.astype('<i2').tobytes())
pcm.astype('<i2').tofile(P/'long_pcm.bin')
# First20ms frame has a zero left half; then exactly one new640/320 frame per block.
padded=np.pad(pcm,(320,0))
frames=np.stack([padded[i:i+640] for i in range(0,len(pcm),320)]).astype(np.float64)/32768
frames*=.5-.5*np.cos(2*np.pi*np.arange(640)/640)
z=np.fft.rfft(frames,n=1024)[:,:512]
features=np.clip(r.nearest(np.log(np.maximum(np.abs(z)@r.B.T,np.finfo(np.float32).tiny))@r.D.T*2),-128,127).astype(np.int8)
features.tofile(P/'features_float.bin')
silent=r.mfcc(np.zeros(16000,dtype=np.int16))[0]
ring=np.tile(silent,(49,1));head=0;trace=[]
for i,feature in enumerate(features):
 ring[head]=feature;head=(head+1)%49;end=(i+1)*320
 if end%1600==0:
  snapshot=np.concatenate([ring[head:],ring[:head]])
  scores,*_=r.network(snapshot)
  trace.append(dict(time_ms=end//16,frame_index=i,head=head,scores=scores.tolist()))
(P/'pc_trace.json').write_text(json.dumps(trace,indent=2),encoding='utf-8')
scores=np.array([a['scores'] for a in trace],dtype=np.int8)
manifest=dict(sample_rate=16000,samples=len(pcm),duration_seconds=len(pcm)/16000,blocks=len(features),inferences=len(trace),sequence=sequence,events=events,selection='First filename-sorted source per class, no prediction filtering',sha256=hashlib.sha256(pcm.astype('<i2').tobytes()).hexdigest())
(P/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
base=bytearray((END/'image.bin').read_bytes());assert len(base)<=0x20000
base+=bytes(0x20000-len(base));base+=pcm.astype('<i2').tobytes()
(P/'native_image.bin').write_bytes(base)
assert len(base)<0x100000
base+=bytes(0x100000-len(base));base+=features.tobytes()
base+=bytes(0x110000-len(base));base+=scores.tobytes()
(P/'control_image.bin').write_bytes(base)
(P/'stream_config.h').write_text('#define STREAM_BLOCKS %d\n#define STREAM_INFERENCES %d\n#define STREAM_SAMPLES %d\n#define STREAM_OUTPUT_BYTES 196608\n#define STREAM_CLOCK_HZ 513000000u\n'%(len(features),len(trace),len(pcm)))
print('Prepared %.1fs, %d new frames, %d inference ticks, %d expected events'%(len(pcm)/16000,len(features),len(trace),len(events)))
