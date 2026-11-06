"""Generate the board's packed SDRAM image and 13-class regression audio."""
import json
from pathlib import Path
import wave

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MODEL = ROOT / 'work/kws13/finetune'
DATA = ROOT / 'data/speech_commands_test_small'
COMMANDS = ('four', 'five', 'six', 'seven', 'eight', 'nine', 'go', 'stop', 'left', 'right', 'off')
LAYERS = [
    ('CONV1', 0, 2, 6), ('CONV2_DS', 1, 2, 5), ('CONV2_PW', 2, 4, 8),
    ('CONV3_DS', 1, 2, 4), ('CONV3_PW', 2, 5, 8), ('CONV4_DS', 1, 3, 5),
    ('CONV4_PW', 2, 5, 7), ('CONV5_DS', 1, 3, 5), ('CONV5_PW', 2, 5, 8),
    ('FINAL_FC', 3, 2, 7),
]


def main():
    labels = json.loads((MODEL / 'report13.json').read_text())['labels']
    assert labels[:2] == ['_silence_', '_unknown_'] and tuple(labels[2:]) == COMMANDS
    actions = ['LED_NONE', 'LED_NONE'] + ['LED_' + word.upper() for word in COMMANDS]
    image = bytearray(65536)
    header = ['#include "led_actions.h"', 'typedef struct {unsigned address,bytes;int kind,bl,rs;signed char bias[64];} Layer;',
              'static const Layer layers[10]={']
    with np.load(MODEL / 'weights13.npz') as model:
        assert model['FINAL_FC_WT'].shape == (13, 64)
        assert model['FINAL_FC_BIAS'].shape == (13,)
        for name, kind, bl, rs in LAYERS:
            weights = model[name + '_WT']
            if kind == 3:
                weights = np.pad(weights, ((0, 1), (0, 0)))
            if kind != 1:
                columns = 40 if kind == 0 else 64
                weights = weights.reshape(-1, 2, columns).transpose(0, 2, 1)
            packed = weights.astype('<i2').tobytes()
            bias = ','.join(map(str, model[name + '_BIAS']))
            header.append(f'{{0x{0x01000000+len(image):08x},{len(packed)},{kind},{bl},{rs},{{{bias}}}}},')
            image.extend(packed)
    header += ['};', 'static const enum LedAction led_actions[13]={' + ','.join(actions) + '};',
               '#ifdef BOARD_CONTINUOUS',
               'static const char *const keyword_names[13]={' + ','.join(map(json.dumps, labels)) + '};',
               '#endif']
    (HERE / 'model13.h').write_text('\n'.join(header) + '\n')
    words = np.frombuffer(image, dtype='<u4')
    with (HERE / 'fixture13.c').open('w') as out:
        out.write('#pragma section("stream_fixture")\nconst unsigned stream_image[]={\n')
        for i in range(0, len(words), 16):
            out.write(','.join(f'0x{w:08x}u' for w in words[i:i+16]) + ',\n')
        out.write('};\n')

    entries = json.loads((DATA / 'manifest.json').read_text())
    with wave.open(str(HERE / 'keywords13.wav'), 'wb') as wav:
        wav.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
        wav.writeframes(bytes(32000))
        for label in labels[2:]:
            entry = next(x for x in entries if x['label'] == label)
            with wave.open(str(DATA / entry['file']), 'rb') as src:
                assert (src.getnchannels(), src.getsampwidth(), src.getframerate()) == (1, 2, 16000)
                wav.writeframes(src.readframes(16000).ljust(32000, b'\0'))
            wav.writeframes(bytes(25600))
    (HERE / 'keywords13.json').write_text(json.dumps({
        'labels': labels, 'sequence': labels[2:],
    }) + '\n')


if __name__ == '__main__':
    main()
