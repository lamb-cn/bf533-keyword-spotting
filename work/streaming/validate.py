from pathlib import Path
import argparse,json,struct,sys,hashlib
import numpy as np
P=Path(__file__).resolve().parent;sys.path.insert(0,str(P.parent/'end_to_end'))
from reference import network,LABELS
parser=argparse.ArgumentParser();parser.add_argument('--mode',choices=['control','native'],required=True);parser.add_argument('--manifest',default='manifest.json');parser.add_argument('--features',default='features_float.bin');parser.add_argument('--weights');args=parser.parse_args()
if args.weights:
 import reference
 reference.W=dict(np.load(P/args.weights))
m=json.loads((P/args.manifest).read_text(encoding='utf-8'))
b=(P/('stream_'+args.mode+'.bin')).read_bytes();assert len(b)==196608
h=struct.unpack_from('<64I',b)
assert h[0]==0x600dcafe,('incomplete',h[:8])
assert h[1]==0,('target errors',h)
assert h[2]==(1 if args.mode=='native' else 0),'wrong build mode'
assert h[34]==0x01000000,'ELF fixture placed at wrong address'
image=(P/(args.mode+'.input.bin')).read_bytes()
pcm=image[0x20000:0x20000+m['samples']*2]
assert hashlib.sha256(pcm).hexdigest()==m['sha256'],'PCM differs from frozen manifest'
assert (h[3],h[4],h[5],h[29])==(m['blocks'],m['blocks'],m['inferences'],m['blocks']),('counts',h[:7])
assert h[14]==m['blocks'],'audio DMA count'
assert h[10]<=h[26] and h[12]==0,'20ms deadline missed'
assert all(h[i]==0 for i in [18,19,20,23,24,28]),'drop/ring/guard/FFT-state errors'
if args.mode=='native':
 assert h[6]==m['inferences'],'some inference ticks skipped real network'
 assert h[15]==m['inferences']*10,'weight IRQ count'
 assert (h[16],h[33])==(m['inferences']-1,m['inferences']-1),'twiddle restoration count'
 assert h[17]==1 and h[30]==1,'repeated FFT init or calibration'
 assert h[21]+h[22]==m['blocks'],'feature update computation count'
 assert h[32]==h[21]+1,'unexpected MFCC recomputation'
else:
 assert h[6]==0 and h[15]==0,'control must be clearly distinguished'
features=np.frombuffer(b,dtype=np.int8,count=m['blocks']*10,offset=2048).reshape(-1,10)
silence=np.frombuffer(b,dtype=np.int8,count=10,offset=1280)
assert silence.tolist()==[-128]+[0]*9
records=np.frombuffer(b,dtype='<u4',count=m['blocks']*8,offset=16384).reshape(-1,8)
assert np.array_equal(records[:,0],np.arange(m['blocks']))
assert np.array_equal(records[:,1],np.arange(1,m['blocks']+1)*20)
assert np.array_equal(records[:,2],np.arange(1,m['blocks']+1)%49)
assert np.array_equal(records[:,3],np.arange(1,m['blocks']+1))
assert np.array_equal(records[:,4],records[:,5]),'previous48 rows changed'
trace=[]
for j in range(m['inferences']):
 off=65536+j*64;r=struct.unpack_from('<16I',b,off)
 assert r[0]==(j+1)*100 and r[1]==(j+1)*5 and r[2]==((j+1)*5)%49 and r[15]==j
 snapshot=np.frombuffer(b,dtype=np.int8,count=490,offset=81920+j*490).reshape(49,10)
 end=(j+1)*5;available=features[max(0,end-49):end]
 expected=np.vstack([np.tile(silence,(49-len(available),1)),available])
 assert np.array_equal(snapshot,expected),('retained48/order',j)
 scores=np.frombuffer(b,dtype=np.int8,count=12,offset=off+16)
 golden,*_=network(snapshot)
 assert np.array_equal(scores,golden),('network scores mismatch',j,scores,golden)
 trace.append(dict(time_ms=r[0],features_seen=r[1],head=r[2],network_cycles=r[3],scores=scores.tolist()))
events=[]
assert h[7]<=64
for j in range(h[7]):
 t,c,v,peak=struct.unpack_from('<IIIi',b,256+j*16)
 assert 2<=c<12 and t%100==0 and v>=2 and peak>=0
 events.append(dict(label=LABELS[c],time_ms=t,votes=v,peak=peak))
assert len(events)==len(m['events']),('missing/duplicate events',events,m['sequence'])
for i,(e,g) in enumerate(zip(events,m['events'])):
 assert e['label']==g['label'],('wrong keyword',i,e,g)
 assert g['start_sample']//16<=e['time_ms']<g['end_sample']//16+650,('event matched wrong occurrence',i,e,g)
gold=np.fromfile(P/args.features,dtype=np.int8).reshape(-1,10)
delta=np.abs(features.astype(int)-gold.astype(int))
report=dict(mode=args.mode,goal_verified=args.mode=='native',duration_seconds=m['duration_seconds'],input_blocks=h[3],new_feature_updates=h[4],feature_fft_calls=h[21],exact_silence_shortcuts=h[22],inference_requests=h[5],actual_native_network_calls=h[6],events=events,duplicate_events=0,missing_events=0,wrong_events=0,deadline_misses=h[12],max_job_cycles=h[10],frame_budget_cycles=h[26],max_network_cycles=h[11],max_feature_cycles=h[13],audio_dma_irqs=h[14],weight_dma_irqs=h[15],twiddle_restore_irqs=h[16],twiddle_crc_checks=h[33],mfcc_calls=h[32],calibration_calls=h[30],busy_cycles=h[8]+(h[9]<<32),feature_vs_float_mae=float(delta.mean()),feature_vs_float_max=int(delta.max()),all_snapshots_preserved_previous48=True,all_inference_scores_match_golden=True,timeline='accelerated causal event-driven: 20ms arrivals and100ms inference; idle cycles omitted, job deadlines checked at configured513MHz; not physical SPORT/timer pacing')
(P/(args.mode+'_report.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(P/(args.mode+'_trace.json')).write_text(json.dumps(trace,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
