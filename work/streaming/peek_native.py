"""Read-only live header inspection for OUR running CCES simulator process.

No write/debug/stop rights are requested. Only matching numeric application
header/stat fields are retained, never a general process dump. This is live,
non-atomic progress evidence; final acceptance still requires validate.py.
"""
from pathlib import Path
import ctypes as c,ctypes.wintypes as w,json,struct,re,datetime
P=Path(__file__).parent
status=json.loads((P/'native.status.json').read_text(encoding='utf-8-sig'));pid=status['runnerPid']
k=c.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.CloseHandle.argtypes=[w.HANDLE]
k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)];k.ReadProcessMemory.restype=w.BOOL
k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)];k.QueryFullProcessImageNameW.restype=w.BOOL
class MBI(c.Structure):
 _fields_=[('BaseAddress',c.c_void_p),('AllocationBase',c.c_void_p),('AllocationProtect',w.DWORD),('PartitionId',w.WORD),('RegionSize',c.c_size_t),('State',w.DWORD),('Protect',w.DWORD),('Type',w.DWORD)]
k.VirtualQueryEx.argtypes=[w.HANDLE,c.c_void_p,c.POINTER(MBI),c.c_size_t];k.VirtualQueryEx.restype=c.c_size_t
class FT(c.Structure):_fields_=[('lo',w.DWORD),('hi',w.DWORD)]
k.GetProcessTimes.argtypes=[w.HANDLE,c.POINTER(FT),c.POINTER(FT),c.POINTER(FT),c.POINTER(FT)];k.GetProcessTimes.restype=w.BOOL
h=k.OpenProcess(0x410,False,pid)
if not h:
 print(json.dumps(dict(pid=pid,state='Unavailable',win32_error=c.get_last_error())));raise SystemExit(0)
try:
 name=c.create_unicode_buffer(1024);length=w.DWORD(1024)
 assert k.QueryFullProcessImageNameW(h,0,name,c.byref(length)) and name.value.lower().endswith('cces_runner.exe')
 created,exited,kernel,user=FT(),FT(),FT(),FT()
 assert k.GetProcessTimes(h,c.byref(created),c.byref(exited),c.byref(kernel),c.byref(user))
 stamp=((created.hi<<32)|created.lo)/1e7-11644473600
 expected=datetime.datetime.fromisoformat(status['runnerStartedUtc'].replace('Z','+00:00')).timestamp()
 assert abs(stamp-expected)<.01,'PID was reused'
 def read(address,count):
  buffer=c.create_string_buffer(count);got=c.c_size_t()
  return buffer.raw if k.ReadProcessMemory(h,address,buffer,count,c.byref(got)) and got.value==count else None
 def header(address):
  raw=read(address,256)
  if raw:
   v=struct.unpack('<64I',raw)
   if v[2]==1 and v[25:28]==(513000000,10260000,21800) and v[31]==1 and v[34]==0x01000000 and v[3]<=1090 and v[4]<=1090 and v[5]<=218:return v
  return None
 candidates=[];cache=P/'native.peek.json'
 if cache.exists():
  old=json.loads(cache.read_text())
  if old.get('pid')==pid:
   for a in old.get('host_header_addresses',[]):
    v=header(a)
    if v:candidates.append((a,v))
 if not candidates:
  needle=struct.pack('<III',513000000,10260000,21800);address=0;scanned=0
  while address<0x100000000 and scanned<512*1024*1024:
   m=MBI()
   if not k.VirtualQueryEx(h,address,c.byref(m),c.sizeof(m)):break
   base=m.BaseAddress or 0;size=m.RegionSize
   if m.State==0x1000 and m.Type==0x20000 and not(m.Protect&0x101) and (m.Protect&0xff) in (2,4,8,0x20,0x40,0x80):
    for off in range(0,size,1024*1024):
     data=read(base+off,min(1024*1024,size-off))
     if data:
      scanned+=len(data);start=0
      while True:
       loc=data.find(needle,start)
       if loc<0:break
       a=base+off+loc-100;v=header(a)
       if v:candidates.append((a,v))
       start=loc+1
   if not size:break
   address=base+size
 syms=(P/'native.symbols.txt').read_text(encoding='utf-8-sig')
 def symbol(n):return int(re.search(r'^'+re.escape(n)+r' = (0x[0-9A-Fa-f]+);',syms,re.M).group(1),16)
 out=[]
 for a,v in candidates:
  state=dict(completed_blocks=v[3],new_features=v[4],inferences=v[5],actual_network_calls=v[6],events=v[7],errors=v[1],max_job_cycles=v[10],deadline_misses=v[12],feature_fft_calls=v[21],silence_shortcuts=v[22],twiddle_crc_checks=v[33])
  # Adjacent globals in the same emulated L1-B RAM page; validate ranges.
  offset=symbol('_stats')-symbol('_header');raw=read(a+offset,256)
  if raw:
   stats=struct.unpack('<64I',raw)
   if stats[2]<=2180 and stats[3]<=1090:state.update(weight_irq_live=stats[2],audio_irq_live=stats[3])
  out.append(state)
 result=dict(pid=pid,read_only=True,observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),host_header_addresses=[a for a,v in candidates],snapshots=out,acceptance_evidence=False)
 cache.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
finally:k.CloseHandle(h)
