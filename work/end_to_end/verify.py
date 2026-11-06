from pathlib import Path
import struct,json,numpy as np,argparse
from reference import network,LABELS,mfcc,read_audio,ROOT
p=Path(__file__).parent
parser=argparse.ArgumentParser();parser.add_argument('--binary',default='target.bin');parser.add_argument('--report',default='target_report.json');args=parser.parse_args()
b=(p/args.binary).read_bytes();assert len(b)==73152
s=struct.unpack_from('<64I',b)
assert s[0]==0x600dcafe,'incomplete run'
assert s[1]==0,('target errors',s[:10])
assert (s[2],s[3])==(10,49 if s[9] in (0,1) else 0),('IRQ counts',s[2:4])
if s[9] in (4,5):assert (s[61],s[62])==(72 if s[9]==5 else 288,72000),('output DMA',s[61:63])
features=np.frombuffer(b,dtype=np.int8,count=490,offset=256).reshape(49,10)
scores,layers,pool=network(features)
for i,expected in enumerate(layers):
 actual=np.frombuffer(b,dtype=np.int8,count=8000,offset=1024+i*8000).reshape(25,5,64)
 assert np.array_equal(actual,expected),('layer mismatch',i,int(np.sum(actual!=expected)))
assert np.array_equal(np.frombuffer(b,dtype=np.int8,count=64,offset=73024),pool)
actual=np.frombuffer(b,dtype=np.int8,count=12,offset=73088)
assert np.array_equal(scores,actual),('FC mismatch',scores,actual)
overlap=[]
for j in range(1,10):
 duration=max(0,min(s[30+j],s[50+j-1])-max(s[20+j],s[40+j-1]))
 overlap.append(dict(transfer=j,cycles=duration,dma_start=s[20+j],dma_done=s[30+j],compute_start=s[40+j-1],compute_end=s[50+j-1]))
meta=json.loads((p/'target_audio.json').read_text(encoding='utf-8'))
audio_path=ROOT/meta['file']
# The historical fixture retains its original PCM after the WAV was replaced.
pcm=read_audio(audio_path) if audio_path.is_file() else np.frombuffer(
    (p/'image.bin').read_bytes(),dtype='<i2',count=16000)
fp=mfcc(pcm)
report=dict(audio=meta['file'],mode=s[9],scores=actual.tolist(),host_predicted=LABELS[int(actual.argmax())],weight_irqs=s[2],audio_irqs=s[3],output_irqs=s[61],output_bytes=s[62],output_wait_cycles=s[63],weight_wait_cycles=s[4],audio_wait_cycles=s[5],mfcc_cycles=s[6],conv_region_cycles=s[7],total_cycles=s[8],pool_cycles=s[60],layer_cycles=list(s[10:20]),prefetch_overlap=overlap,mfcc_vs_float_mae=float(np.abs(features.astype(int)-fp.astype(int)).mean()),mfcc_vs_float_max=int(np.abs(features.astype(int)-fp.astype(int)).max()),verified_network_outputs=72000+64+12)
(p/args.report).write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
