from pathlib import Path
import argparse,struct,re
P=Path(__file__).parent
parser=argparse.ArgumentParser();parser.add_argument('--tag',choices=['control','native'],required=True);a=parser.parse_args()
b=(P/(a.tag+'.input.bin')).read_bytes();assert len(b)%4==0 and len(b)<=0x200000
words=struct.unpack('<%dI'%(len(b)//4),b)
text='#pragma section("stream_fixture")\nconst unsigned stream_image[%d]={\n'%len(words)
text+='\n'.join(','.join('0x%08xu'%x for x in words[i:i+16])+',' for i in range(0,len(words),16))+'\n};\n'
(P/('fixture_'+a.tag+'.c')).write_text(text)
ld=Path(r'C:/Analog Devices/CrossCore Embedded Studio 2.10.0/Blackfin/ldf/ADSP-BF533.ldf')
s='#define STACK_SIZE 4096\n#define HEAP_SIZE 1024\n#define STACKHEAP_SIZE 5120\n'+ld.read_text()
s=s.replace('MEMORY {','MEMORY {\n  MEM_STREAM_FIXTURE { START(0x01000000) END(0x011FFFFF) TYPE(RAM) WIDTH(8) }',1)
s=s.replace('MEM_SDRAM0_BANK1     { START(0x01000000)','MEM_SDRAM0_BANK1     { START(0x01200000)')
s=s.replace('  SECTIONS {','  SECTIONS {\n    stream_fixture { INPUT_SECTION_ALIGN(4) INPUT_SECTIONS($OBJS_LIBS(stream_fixture)) } >MEM_STREAM_FIXTURE',1)
s=re.sub(r'#include\s+"([^\"]+)"',lambda m:'#include "'+str(ld.parent/m.group(1)).replace('\\','/')+'"' if (ld.parent/m.group(1)).exists() else m.group(0),s)
(P/'embedded.ldf').write_text(s)
print('Embedded %s fixture: %d bytes at0x01000000'%(a.tag,len(b)))
