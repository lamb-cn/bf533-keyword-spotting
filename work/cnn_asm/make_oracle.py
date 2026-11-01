"""Independent host reference. Same LCG fixture stream as bench.c, integer math.
Writes 15*8000 int32 raw sums then 15*8000 int8 final outputs.
No packed weights, padded input or assembly implementation is used here.
"""
import pathlib
import struct

state = 0x12345678
def value(mode):
    global state
    state = (state * 1664525 + 1013904223) & 0xffffffff
    if mode == 1:
        return 0
    if mode == 2:
        return -128 if state & 0x10000 else 127
    return (state >> 24) - 128

cases = [(0,0,2,6),(1,0,2,5),(2,0,4,8),(1,0,2,4),(2,0,5,8),
         (1,0,3,5),(2,0,5,7),(1,0,3,5),(2,0,5,8)]
cases += [(k,1,2,6) for k in range(3)]
cases += [(k,2,2,6) for k in range(3)]
all_raw, all_final = [], bytearray()
for kind, mode, bl, rs in cases:
    src = [value(mode) for _ in range(8000)]
    w = [value(mode) for _ in range(4096)]
    bias = [value(0) for _ in range(64)]
    for oy in range(25):
        for ox in range(5):
            for c in range(64):
                total = 0
                if kind == 2:
                    for ic in range(64):
                        total += src[(oy*5+ox)*64+ic] * w[c*64+ic]
                else:
                    for ky in range(10 if kind == 0 else 3):
                        for kx in range(4 if kind == 0 else 3):
                            iy = oy*(2 if kind == 0 else 1)+ky-(4 if kind == 0 else 1)
                            ix = ox*(2 if kind == 0 else 1)+kx-1
                            if 0 <= iy < (49 if kind == 0 else 25) and 0 <= ix < (10 if kind == 0 else 5):
                                if kind == 0:
                                    total += src[iy*10+ix] * w[c*40+ky*4+kx]
                                else:
                                    total += src[(iy*5+ix)*64+c] * w[(ky*3+kx)*64+c]
                all_raw.append(total)
                q = (total + bias[c]*(1 << bl) + (1 << (rs-1))) // (1 << rs)
                q = min(127, max(-128, q))
                if mode != 2:
                    q = max(0, q)
                all_final.append(q & 255)
out = pathlib.Path(__file__).with_name('oracle.bin')
out.write_bytes(struct.pack('<120000i', *all_raw) + all_final)
print(f'{out.name}: {len(all_raw)} independent raw sums and {len(all_final)} final outputs')
