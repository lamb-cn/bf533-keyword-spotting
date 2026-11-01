"""Exhaustive mathematical check for the exact bounded-domain ASM reciprocal.
Target tests separately exercise the actual instructions on full HWC tensors.
"""
for s in range(-16000,15876):
    n=abs(s)*4
    q=(n*33554)>>22
    q+=n-q*125>=125
    q=-q if s<0 else q
    expected=(abs(s)*4)//125
    if s<0:expected=-expected
    assert q==expected,(s,q,expected)
print('PASS: all 31876 attainable pool sums, exact truncation toward zero')
