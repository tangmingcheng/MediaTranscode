# rk520 离线取证命令参数

以下均以 `D:/Python/python.exe -c <程序>` 直接执行，没有创建Windows测试脚本；输出均为本轮临时分析产物，验收记录完成后清理。协议方法沿r452，按RFC3550扩展序号重排；AAC使用本轮真实fmtp config=1210。首版按到达顺序重组在FU-A报错，最终程序保留乱序计数并检查序号缺口，不把乱序误判为丢包。此文件记录实际程序，不构成仓库测试体系。

## 协议提取（按SSRC扩展序号重排，记录乱序与缺口）

```python
import struct,pathlib,json,collections
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
prefix='composition-rk520-'
raw_packets=[]; transport=[]; streams={}; sr=[]; bye=[]; rtcp=collections.Counter(); packets=0; tscc={}; ccerrors=[]; tei=0
def stream(port,ssrc):
 k=(port,ssrc)
 if k not in streams:
  ext='ts' if port==62024 else ('h264' if port==61020 else 'aac')
  name=prefix+str(port)+'-'+str(ssrc)+'.'+ext
  streams[k]=dict(port=port,ssrc=ssrc,file=name,last=None,gaps=[],packets=0,units=[],au=bytearray(),fu=None,vt=None,frag=None,handle=(p/name).open('wb'))
 return streams[k]
def audio_write(s,ts,data):
 n=len(data)+7
 adts=bytes([255,241,80,128|(n>>11),(n>>3)&255,((n&7)<<5)|31,252])
 s['handle'].write(adts+data);s['units'].append(dict(rtp=ts,bytes=len(data)))
with (p/(prefix+'joint.pcap')).open('rb') as f:
 hdr=f.read(24);assert hdr[:4]==b'\xd4\xc3\xb2\xa1' and struct.unpack('<I',hdr[20:24])[0]==1
 while True:
  h=f.read(16)
  if not h:break
  assert len(h)==16
  sec,usec,n,orig=struct.unpack('<IIII',h);b=f.read(n);assert len(b)==n and n==orig
  packets+=1;eth=int.from_bytes(b[12:14],'big');off=14
  while eth in (0x8100,0x88a8):eth=int.from_bytes(b[off+2:off+4],'big');off+=4
  assert eth==0x800
  ip=b[off:];assert ip[9]==17 and int.from_bytes(ip[6:8],'big')&0x3fff==0
  udp=ip[(ip[0]&15)*4:];port=int.from_bytes(udp[2:4],'big');size=int.from_bytes(udp[4:6],'big');q=udp[8:size]
  assert len(q)==size-8
  if port in (61021,61023,62025):
   pos=0
   while pos<len(q):
    ln=4*(int.from_bytes(q[pos+2:pos+4],'big')+1);r=q[pos:pos+ln];assert len(r)==ln;typ=r[1];rtcp[str(typ)]+=1
    if typ==200:sr.append(dict(capture_ns=sec*1000000000+usec*1000,port=port,ssrc=int.from_bytes(r[4:8],'big'),seconds=int.from_bytes(r[8:12],'big'),fraction=int.from_bytes(r[12:16],'big'),rtp=int.from_bytes(r[16:20],'big')))
    if typ==203:bye.append(dict(port=port,ssrc=int.from_bytes(r[4:8],'big'),capture=sec+usec/1e6))
    pos+=ln
   continue
  raw_packets.append((sec,usec,port,q))
groups={}
for sec,usec,port,q in raw_packets:
 key=(port,int.from_bytes(q[8:12],'big'));seq=int.from_bytes(q[2:4],'big')
 g=groups.setdefault(key,dict(high=seq,entries=[],arrival_reorders=0))
 ext=g['high']+((seq-g['high']+32768)%65536)-32768
 if ext<g['high']:g['arrival_reorders']+=1
 g['high']=max(g['high'],ext);g['entries'].append((ext,sec,usec,port,q))
ordered=[]
for key,g in groups.items():
 entries=sorted(g['entries'],key=lambda a:a[0])
 assert len({e[0] for e in entries})==len(entries),('duplicates',key)
 missing=[(a[0],b[0]) for a,b in zip(entries,entries[1:]) if b[0]!=a[0]+1]
 transport.append(dict(port=key[0],ssrc=key[1],arrival_reorders=g['arrival_reorders'],missing=missing))
 ordered.extend(e[1:] for e in entries)
for sec,usec,port,q in ordered:
 assert port in (61020,61022,62024) and q[0]>>6==2
 seq=int.from_bytes(q[2:4],'big');ts=int.from_bytes(q[4:8],'big');ssrc=int.from_bytes(q[8:12],'big');s=stream(port,ssrc)
 if s['last'] is not None and seq!=(s['last']+1)%65536:s['gaps'].append([s['last'],seq])
 s['last']=seq;s['packets']+=1;pos=12+4*(q[0]&15)
 if q[0]&16:pos+=4+4*int.from_bytes(q[pos+2:pos+4],'big')
 pay=q[pos:len(q)-(q[-1] if q[0]&32 else 0)]
 if port==62024:
  assert len(pay)%188==0
  s['handle'].write(pay)
  for j in range(0,len(pay),188):
   t=pay[j:j+188];assert t[0]==71;tei+=bool(t[1]&128);pid=((t[1]&31)<<8)|t[2];af=(t[3]>>4)&3;cc=t[3]&15
   discontinuity=bool(af&2 and t[4]>0 and t[5]&128)
   if pid!=8191 and af&1:
    if pid in tscc and not discontinuity and cc!=(tscc[pid]+1)%16:ccerrors.append([pid,tscc[pid],cc])
    tscc[pid]=cc
  continue
 if port==61020:
  if s['vt'] is None:s['vt']=ts
  assert ts==s['vt'];typ=pay[0]&31
  if 1<=typ<=23:s['au']+=b'\0\0\0\1'+pay
  elif typ==24:
   x=1
   while x<len(pay):
    n=int.from_bytes(pay[x:x+2],'big');x+=2;assert x+n<=len(pay);s['au']+=b'\0\0\0\1'+pay[x:x+n];x+=n
  elif typ==28:
   if pay[1]&128:assert s['fu'] is None;s['fu']=bytearray([(pay[0]&224)|(pay[1]&31)])
   assert s['fu'] is not None, (packets,port,ssrc,seq,ts,s['gaps'][-10:],pay[:8].hex());s['fu']+=pay[2:]
   if pay[1]&64:s['au']+=b'\0\0\0\1'+s['fu'];s['fu']=None
  else:raise ValueError(typ)
  if q[1]&128:
   assert s['fu'] is None;s['handle'].write(s['au']);s['units'].append(dict(rtp=ts,bytes=len(s['au'])));s['au'].clear();s['vt']=None
 else:
  bits=int.from_bytes(pay[:2],'big');assert bits%16==0;count=bits//16;x=2+count*2
  if count==1:
   ah=int.from_bytes(pay[2:4],'big');n=ah>>3;assert ah&7==0
   if s['frag'] is not None or n>len(pay)-x:
    if s['frag'] is None:s['frag']=[ts,n,bytearray()]
    assert s['frag'][:2]==[ts,n];s['frag'][2]+=pay[x:];assert len(s['frag'][2])<=n
    if q[1]&128:
     assert len(s['frag'][2])==n;audio_write(s,ts,s['frag'][2]);s['frag']=None
    continue
  assert s['frag'] is None
  for i in range(count):
   ah=int.from_bytes(pay[2+i*2:4+i*2],'big');n=ah>>3;assert ah&7==0;data=pay[x:x+n];assert len(data)==n;x+=n;audio_write(s,(ts+i*1024)%2**32,data)
  assert x==len(pay)
out=[]
for s in streams.values():
 s['incomplete_tail']=dict(video_au_bytes=len(s['au']),video_fragment_bytes=0 if s['fu'] is None else len(s['fu']),audio_fragment_bytes=0 if s['frag'] is None else len(s['frag'][2]))
 s.pop('handle').close()
 for k in ('au','fu','vt','frag'):s.pop(k)
 out.append(s)
result=dict(transport=transport,packets=packets,streams=out,sr=sr,bye=bye,rtcp=dict(rtcp),ts_cc_errors=ccerrors,tei=tei)
(p/(prefix+'packet-map.json')).write_text(json.dumps(result))
print(json.dumps(dict(transport=transport,packets=packets,streams=[{k:v for k,v in s.items() if k!='units'}|dict(units=len(s['units'])) for s in out],sr=len(sr),bye=bye,rtcp=dict(rtcp),ccerrors=len(ccerrors),tei=tei)))

```

## PCM与原始TS检查

```python
import pathlib,subprocess,json,re
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance');pre='composition-rk520-'; ff='D:/mabs/local64/bin-video/'
paths={'reference':p/'test-continuous-120s.mp4','input1':p/(pre+'61022-1362614934.aac'),'output':p/(pre+'62024-696190908.ts')}
meta={}
for name,path in paths.items():
 cmd=[ff+'ffmpeg.exe','-hide_banner','-nostdin','-y','-copyts','-i',str(path),'-map','0:a:0','-vn','-af','aformat=sample_fmts=flt:sample_rates=44100:channel_layouts=mono,ashowinfo','-c:a','pcm_f32le','-f','f32le',str(p/(pre+name+'.f32'))]
 r=subprocess.run(cmd,capture_output=True);log=r.stderr.decode(errors='replace');(p/(pre+name+'-audio.log')).write_text(log)
 assert r.returncode==0
 blocks=[];total=0
 for line in log.splitlines():
  if 'Parsed_ashowinfo' not in line or 'nb_samples:' not in line:continue
  pts=int(re.search(r'pts:(-?\d+)',line)[1]);n=int(re.search(r'nb_samples:(\d+)',line)[1]);blocks.append(dict(index=total,pts=pts,n=n));total+=n
 gaps=[i for i in range(1,len(blocks)) if blocks[i]['pts']!=blocks[i-1]['pts']+blocks[i-1]['n']]
 meta[name]=dict(blocks=blocks,samples=total,gaps=gaps)
 print(name,total,'first',blocks[0],'last',blocks[-1],'gaps',len(gaps))
(p/(pre+'audio-map.json')).write_text(json.dumps(meta))
r=subprocess.run([ff+'ffmpeg.exe','-hide_banner','-nostdin','-v','warning','-i',str(paths['output']),'-map','0:v:0','-map','0:a:0','-f','null','-'],capture_output=True)
(p/(pre+'wire-decode.log')).write_bytes(r.stderr);print('WIRE_DECODE_EXIT',r.returncode,'WARNING_BYTES',len(r.stderr),r.stderr.decode(errors='replace')[-1500:])
r=subprocess.run([ff+'ffprobe.exe','-v','error','-show_packets','-show_data_hash','sha256','-show_entries','packet=stream_index,pts,dts,pts_time,data_hash','-of','json',str(paths['output'])],capture_output=True,check=True)
(p/(pre+'wire-packets.json')).write_bytes(r.stdout);pack=json.loads(r.stdout)['packets']
for idx in sorted({x['stream_index'] for x in pack}):
 q=[x for x in pack if x['stream_index']==idx];err=sum(int(b['dts'])<=int(a['dts']) for a,b in zip(q,q[1:]));print('STREAM',idx,'PACKETS',len(q),'DTS_NONINCREASE',err)

```

## 首代时钟及局部内容对应

```python
import pathlib,json,numpy as np
from scipy.signal import correlate,find_peaks
from fractions import Fraction
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance');pre='composition-rk520-'
m=json.loads((p/(pre+'packet-map.json')).read_text());a=np.fromfile(p/(pre+'input1.f32'),dtype='<f4');r=np.fromfile(p/(pre+'reference.f32'),dtype='<f4').astype(np.float64);o=np.fromfile(p/(pre+'output.f32'),dtype='<f4').astype(np.float64)
print('INPUT_EQUAL_AFTER_SKIP',np.array_equal(a[1024:],r[:len(a)-1024]),'SAMPLES',len(a)-1024,'REFERENCE_TAIL_NOT_SENT',len(r)-(len(a)-1024))
epochs={}
for port,ssrc,rate,step in [(61020,3370658339,90000,3000),(61022,1362614934,44100,1024)]:
 s=next(x for x in m['streams'] if x['ssrc']==ssrc);u=s['units'];base=u[0]['rtp'];v=sorted((x['rtp']-base)%2**32 for x in u);print('STEP',port,v==[i*step for i in range(len(v))])
 q=[x for x in m['sr'] if x['ssrc']==ssrc];ep=[Fraction(x['seconds'])+Fraction(x['fraction'],2**32)+Fraction(((base-x['rtp']+2**31)%2**32)-2**31,rate) for x in q];epochs[port]=ep;print('SR',port,len(q),'spread_us',float((max(ep)-min(ep))*1000000))
bias=[v-a-Fraction(1024,44100) for v in epochs[61020] for a in epochs[61022]]
print('INPUT_BIAS_MS',float(min(bias)*1000),float(max(bias)*1000))
fs=44100;n=fs;energy=np.r_[0,np.cumsum(r*r)];norm=energy[n:]-energy[:-n];rows=[]
for t in [1,10,30,60,90,100,105,110,115,120]:
 w=o[t*fs:(t+1)*fs];we=np.dot(w,w)
 if we==0:print('SILENT',t);continue
 c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum(norm*we,np.finfo(float).tiny));j=int(np.argmax(c));peaks,_=find_peaks(c,distance=fs);best=sorted(peaks,key=lambda x:c[x],reverse=True)[:3]
 row=dict(output_pts=t,source_audio_pts=j/fs,lag_samples=j-t*fs,correlation=float(c[j]),rms=float(np.sqrt(we/n)),alternates=[dict(source_pts=q/fs,correlation=float(c[q])) for q in best]);rows.append(row);print(json.dumps(row))
(p/(pre+'content-preliminary.json')).write_text(json.dumps(dict(input_bias_ms=[float(min(bias)*1000),float(max(bias)*1000)],rows=rows)))

```

## 整段与留出内容对应

```python
import pathlib,numpy as np,json
from scipy.signal import correlate,find_peaks
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance');pre='composition-rk520-';fs=44100
r=np.fromfile(p/(pre+'reference.f32'),dtype='<f4').astype(np.float64);o=np.fromfile(p/(pre+'output.f32'),dtype='<f4').astype(np.float64)
k=fs;n=105*fs;w=o[k:k+n];en=np.r_[0,np.cumsum(r*r)];c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum((en[n:]-en[:-n])*np.dot(w,w),1e-300));j=int(np.argmax(c));peaks,_=find_peaks(c,distance=fs)
res=dict(output_begin=1,seconds=105,source_begin=j/fs,lag_samples=j-k,correlation=float(c[j]),competitors=[dict(source_begin=q/fs,correlation=float(c[q])) for q in sorted(peaks,key=lambda q:c[q],reverse=True)[:5]])
print(json.dumps(res));(p/(pre+'long-match.json')).write_text(json.dumps(res))
for t in [106,107,108,109]:
 w=o[t*fs:(t+1)*fs];n=len(w);c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum((en[n:]-en[:-n])*np.dot(w,w),1e-300));j=int(np.argmax(c));print('HOLDOUT',t,'source',j/fs,'lag',j-t*fs,'corr',float(c[j]))

```

