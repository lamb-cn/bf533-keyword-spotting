import urllib.request, tarfile, pathlib, wave, hashlib, csv, json
from collections import Counter
root=pathlib.Path('data/speech_commands_test_small')
url='https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_test_set_v0.02.tar.gz'
labels=['yes','no','up','down','left','right','on','off','stop','go','_silence_','_unknown_']
counts=Counter(); rows=[]
with urllib.request.urlopen(url,timeout=90) as stream:
 with tarfile.open(fileobj=stream,mode='r|gz') as archive:
  for member in archive:
   parts=pathlib.PurePosixPath(member.name).parts
   if not member.isfile() or not member.name.lower().endswith('.wav') or len(parts)<2:continue
   label=parts[-2]
   if label not in labels or counts[label]>=5:continue
   content=archive.extractfile(member).read()
   folder=root/label;folder.mkdir(exist_ok=True)
   path=folder/parts[-1];path.write_bytes(content)
   with wave.open(str(path),'rb') as w:
    rate,channels,width,frames=w.getframerate(),w.getnchannels(),w.getsampwidth(),w.getnframes()
   assert rate==16000 and channels==1 and width==2,(path,rate,channels,width)
   rows.append(dict(file=path.relative_to(root).as_posix(),label=label,sample_rate=rate,channels=channels,bits=width*8,samples=frames,duration_seconds=frames/rate,bytes=len(content),sha256=hashlib.sha256(content).hexdigest(),archive_member=member.name))
   counts[label]+=1
   print(label,counts[label],flush=True)
   if all(counts[k]>=5 for k in labels):break
with (root/'manifest.csv').open('w',encoding='utf-8-sig',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
(root/'source.json').write_text(json.dumps(dict(source_url=url,split='official standalone test set v0.02',selection='first 5 WAV members per class in archive order, not random',counts=dict(counts),total=len(rows)),indent=2),encoding='utf-8')
print('TOTAL',len(rows),'BYTES',sum(r['bytes'] for r in rows),dict(counts))
assert len(rows)==60,dict(counts)

