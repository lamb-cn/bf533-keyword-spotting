"""Freeze one feature matrix to isolate weight scheduling from MFCC cost."""
from pathlib import Path
import json,argparse
from reference import mfcc,read_audio,ROOT
p=Path(__file__).parent
parser=argparse.ArgumentParser();parser.add_argument('--native',action='store_true');args=parser.parse_args()
image=bytearray((p/'image.bin').read_bytes())
if args.native:
 features=(p/'target.bin').read_bytes()[256:746];source='BF533 target.bin'
else:
 meta=json.loads((p/'target_audio.json').read_text(encoding='utf-8'))
 features=mfcc(read_audio(ROOT/meta['file'])).tobytes();source='PC floating MFCC reference'
assert len(features)==490
image[32000:32490]=features;(p/'image_replay.bin').write_bytes(image)
(p/'replay_source.json').write_text(json.dumps(dict(feature_source=source)),encoding='utf-8')
print('Frozen replay input:',source)
