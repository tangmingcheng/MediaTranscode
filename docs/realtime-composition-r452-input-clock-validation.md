# r452：选中源与输出联合 RTP/RTCP 取证

状态：联合取证结束，取得有限内容映射与输入基准归因证据，完整目标尚未完成。保持r451四源完整120秒及标记视频H264 NVENC CBR8200kbps、音频copy，输出H264 720p30 VBR8–12Mbps/AAC192kbps MPEG-TS/RTP不变；仅扩大捕获至61020/21/22/23与62024/25。目标定位首代候选偏移来源，本轮不做重入，不替代r451双代功能记录。Windows FFmpeg及生产源码均不修改。

## 实际命令

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 11 -f "udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r452 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r452-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

PID：capture38860，source0/1/2/3=44432/8308/35792/31112，CLI44592，VLC34676。

## 实测结论（有限内容证据，非完整交付）

四个源均完整3600帧/120秒并exit0。VLC原生快照确认四格和全部失源黑屏；CLI精确身份验证后CtrlC，caller_stop/exit0，VLC quit0、dumpcap CtrlC0。7个精确PID核查均不存在。

联合捕获204990包、drop0。输入RTP缺口0，视频3600AU，AAC5168AU；50个SR（视频/音频各25）、2个BYE，未观察SDES（不虚构CNAME证据）。输出90557包、566648个TS包、106529824字节，RTP缺口/CC错误/TEI均0。此为接收侧完整性，不等于NIC实际发送服务证明。

| 项目 | 本轮实测 |
|---|---|
| 输入视频源零点与音频参考零点的共同NTP差 | 208.770974869–208.791383416ms |
| 输出内容视频相对音频偏移 | 225.804988662ms，六个采样时刻1/10/30/60/90/105秒一致 |
| 扣除输入映射偏差后的残差 | 17.013605247–17.034013794ms；30fps帧间隔为33.333ms，此比较不是新设通过阈值 |
| 110秒音频独立整段匹配 | 输出PTS1..111对原源起点5.625804989秒，相关0.998703404；其他间隔至少1秒局部峰最高0.070474925，未按视频筛峰 |

因此本轮约226ms输出偏差的大部分已在发送端SR映射中存在，当前证据不支持对合屏添加约230ms补偿或修改Windows FFmpeg。r451两代只有输出捕获，仍不反推其精确输入偏差；本轮也不证明所有源/设备的首次有效输出、分位数或长时漂移。

### 输入内容与时钟对应

输入解码原始AAC有5292032样本，参考MP4解码5292000。ffprobe实际首包PTS=-1024、Skip Samples=1024；按该真实事实，输入[1024:]与参考[:5291008]逐样本全等。输入未覆盖参考末992样本，不能说整段音频逐样本全部交付。1024不是猜测编码延迟，更未重复从已经裁剪的参考/输出PCM扣除。

输入视频解码图帧0/300/900的标记分别0/10/30秒；视频RTP按呈现时戳排序覆盖3600个连续3000步长，传输顺序存在B帧重排，不能用传输序号当呈现帧号。AAC各AU时戳1024步长。以各SR将输入媒体时戳映射共同NTP，音频参考零点落输入第1024样本；使用Fraction精确计算，不拿到达时刻作媒体时钟。

依据：[RFC3550 SR](https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1)、[RFC6184 H264](https://www.rfc-editor.org/rfc/rfc6184)、[RFC3640 AAC](https://www.rfc-editor.org/rfc/rfc3640)。本机只读rtpenc.c128/134/325/567及ffmpeg_sched.c1215确认各mux header独立建立默认NTP基准；官方[rtpenc.c](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavformat/rtpenc.c)同机制。此为默认发送机制和本轮输入事实，不是Windows FFmpeg启动故障。

### 诊断失败保留

初版离线AAC解析器假定AU不分片，在seq3291、AU1190字节/当前payload1184字节、marker=false处断言失败；补齐RFC3640分片重组后成功，实际122个分片包。初次直接比较输入PCM与已裁剪参考失败，随后依据实际Skip Samples与全量逐位相等校正分析轴；初次要求视频传输顺序时戳单调失败，实际为B帧重排，改为呈现时戳排序并核完整步长。以上均为离线取证假设失败，不是生产进程或网络失败，未隐藏。

## 离线实际程序与命令

以下通过D:/Python/python.exe -c执行，未保留测试脚本文件。

```python
import pathlib,subprocess,json
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
r=subprocess.run(['D:/Wireshark/tshark.exe','-r',str(p/'composition-r452-output.pcapng'),'-Y','udp.dstport==61020 || udp.dstport==61021 || udp.dstport==61022 || udp.dstport==61023','-T','fields','-e','udp.dstport','-e','udp.payload'],capture_output=True,check=True)
sr=[];last={};gap=0;va=[];aa=[];au=bytearray();fu=None;vt=None;types={};rtcp_types={}; audio_frag=None; fragment_packets=0
vf=(p/'composition-r452-input.h264').open('wb');af=(p/'composition-r452-input.aac').open('wb')
for line in r.stdout.decode().splitlines():
 if not line or line=='No matching RTP/H.264 payload dumped.': continue
 port,h=line.split('\t');port=int(port);b=bytes.fromhex(h.replace(':',''))
 if port in (61021,61023):
  pos=0
  while pos<len(b):
   length=4*(int.from_bytes(b[pos+2:pos+4],'big')+1);q=b[pos:pos+length]; assert len(q)==length
   typ=q[1];rtcp_types[str(typ)]=rtcp_types.get(str(typ),0)+1
   if typ==200:
    sr.append(dict(port=port,ssrc=int.from_bytes(q[4:8],'big'),seconds=int.from_bytes(q[8:12],'big'),fraction=int.from_bytes(q[12:16],'big'),rtp=int.from_bytes(q[16:20],'big')))
   pos+=length
  continue
 seq=int.from_bytes(b[2:4],'big');ts=int.from_bytes(b[4:8],'big');ssrc=int.from_bytes(b[8:12],'big');key=(port,ssrc)
 if key in last and seq!=(last[key]+1)%65536:gap+=1
 last[key]=seq;pos=12+4*(b[0]&15)
 if b[0]&16:pos+=4+4*int.from_bytes(b[pos+2:pos+4],'big')
 payload=b[pos:len(b)-(b[-1] if b[0]&32 else 0)]
 if port==61020:
  if vt is None:vt=ts
  assert ts==vt
  typ=payload[0]&31;types[str(typ)]=types.get(str(typ),0)+1
  if 1<=typ<=23:au+=b'\x00\x00\x00\x01'+payload
  elif typ==24:
   x=1
   while x<len(payload):
    n=int.from_bytes(payload[x:x+2],'big');x+=2;assert x+n<=len(payload);au+=b'\x00\x00\x00\x01'+payload[x:x+n];x+=n
  elif typ==28:
   if payload[1]&128:
    assert fu is None;fu=bytearray([(payload[0]&224)|(payload[1]&31)])
   assert fu is not None;fu+=payload[2:]
   if payload[1]&64:au+=b'\x00\x00\x00\x01'+fu;fu=None
  else:raise ValueError(('NAL type',typ))
  if b[1]&128:
   assert fu is None;va.append(dict(index=len(va),rtp=ts,ssrc=ssrc,bytes=len(au)));vf.write(au);au.clear();vt=None
 else:
  bits=int.from_bytes(payload[:2],'big'); assert bits%16==0
  count=bits//16;x=2+count*2
  if count==1:
   h=int.from_bytes(payload[2:4],'big'); size=h>>3; assert h&7==0
   if audio_frag is not None or size>len(payload)-x:
    fragment_packets+=1
    if audio_frag is None: audio_frag=[ts,ssrc,size,bytearray()]
    assert audio_frag[:3]==[ts,ssrc,size]
    audio_frag[3]+=payload[x:]
    assert len(audio_frag[3])<=size
    if b[1]&128:
     assert len(audio_frag[3])==size
     n=size+7; adts=bytes([255,241,(1<<6)|(4<<2),(2<<6)|(n>>11),(n>>3)&255,((n&7)<<5)|31,252])
     af.write(adts+audio_frag[3]);aa.append(dict(index=len(aa),rtp=ts,ssrc=ssrc,bytes=size));audio_frag=None
    continue
  assert audio_frag is None
  for i in range(count):
   h=int.from_bytes(payload[2+i*2:4+i*2],'big');size=h>>3;assert h&7==0
   data=payload[x:x+size];assert len(data)==size;x+=size;n=size+7
   adts=bytes([255,241,(1<<6)|(4<<2), (2<<6)|(n>>11), (n>>3)&255,((n&7)<<5)|31,252])
   af.write(adts+data);aa.append(dict(index=len(aa),rtp=(ts+i*1024)%2**32,ssrc=ssrc,bytes=size))
  assert x==len(payload)
assert not au and fu is None and audio_frag is None
vf.close();af.close()
out=dict(fragment_packets=fragment_packets,sr=sr,video=va,audio=aa,rtp_gaps=gap,nal_types=types,rtcp_types=rtcp_types,stderr=r.stderr.decode())
(p/'composition-r452-input-map.json').write_text(json.dumps(out))
print(json.dumps(dict(sr=len(sr),video=len(va),audio=len(aa),gaps=gap,nal_types=types,rtcp_types=rtcp_types,first_video=va[:1],first_audio=aa[:1])))

```

```python
import subprocess,pathlib,re,json
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
meta={}
for name,path in [('reference',p/'test-continuous-120s.mp4'),('received',p/'composition-r452-output.ts'),('input',p/'composition-r452-input.aac')]:
 cmd=['D:/mabs/local64/bin-video/ffmpeg.exe','-hide_banner','-nostdin','-y','-copyts','-i',str(path),'-map','0:a:0','-vn','-af','aformat=sample_fmts=flt:sample_rates=44100:channel_layouts=mono,ashowinfo','-c:a','pcm_f32le','-f','f32le',str(p/('composition-r452-'+name+'.f32'))]
 r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 log=r.stderr.decode(errors='replace'); (p/('composition-r452-'+name+'-audio.log')).write_text(log)
 assert r.returncode==0,log[-2000:]
 blocks=[]; total=0
 for line in log.splitlines():
  if 'Parsed_ashowinfo' not in line or 'nb_samples:' not in line: continue
  pts=int(re.search(r'pts:(-?\d+)',line)[1]); rate=int(re.search(r'rate:(\d+)',line)[1]); n=int(re.search(r'nb_samples:(\d+)',line)[1])
  blocks.append(dict(index=total,pts=pts,rate=rate,n=n)); total+=n
 assert total*4==(p/('composition-r452-'+name+'.f32')).stat().st_size
 gaps=[i for i in range(1,len(blocks)) if blocks[i]['pts'] != blocks[i-1]['pts']+blocks[i-1]['n']]
 meta[name]=dict(command=cmd,blocks=blocks,samples=total,gaps=gaps)
 print(name,'samples',total,'blocks',len(blocks),'first',blocks[0],'gaps',len(gaps))
(p/'composition-r452-audio-map.json').write_text(json.dumps(meta))

```

```python
import pathlib,json,numpy as np
from scipy.signal import correlate,find_peaks
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
m=json.loads((p/'composition-r452-audio-map.json').read_text())
assert not m['reference']['gaps'] and not m['received']['gaps']
r=np.fromfile(p/'composition-r452-reference.f32',dtype='<f4').astype(np.float64)
o=np.fromfile(p/'composition-r452-received.f32',dtype='<f4').astype(np.float64)
fs=44100;n=44100;energy=np.r_[0,np.cumsum(r*r)]; norm=energy[n:]-energy[:-n]
times=[1,10,30,60,90,105]; markers=[162,432,1032,1932,2832,3282]
rows=[]
for t,frame in zip(times,markers):
 k=round((t-m['received']['blocks'][0]['pts']/fs)*fs); w=o[k:k+n]; we=np.dot(w,w)
 assert len(w)==n and we>0
 c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum(norm*we,np.finfo(float).tiny))
 j=int(np.argmax(c)); peaks,_=find_peaks(c,distance=fs)
 best=sorted(peaks,key=lambda x:c[x],reverse=True)[:5]
 sr=m['reference']['blocks'][0]['pts']/fs+j/fs
 da=t-sr; dv=t-frame/30
 row=dict(output_pts=t,video_frame=frame,source_video_pts=frame/30,source_audio_pts=sr,audio_offset=da,video_offset=dv,video_minus_audio_ms=(dv-da)*1000,correlation=float(c[j]),other_peaks=[dict(source_time=float(q/fs),correlation=float(c[q])) for q in best])
 rows.append(row)
print(json.dumps(rows,indent=2))
(p/'composition-r452-content-result.json').write_text(json.dumps(rows,indent=2))

```

```python
import pathlib,json,numpy as np
from fractions import Fraction
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
m=json.loads((p/'composition-r452-input-map.json').read_text())
a=np.fromfile(p/'composition-r452-input.f32',dtype='<f4');r=np.fromfile(p/'composition-r452-reference.f32',dtype='<f4')
shift=1024 # verified MP4 Skip Samples side data and full decoded equality below
assert np.array_equal(a[shift:],r[:len(a)-shift])
print('exact_equal_samples',len(a)-shift,'reference_unmatched_tail',len(r)-(len(a)-shift))
for key,rate in [('video',90000),('audio',44100)]:
 entries=m[key];base=entries[0]['rtp']; step=3000 if key=='video' else 1024
 assert sorted((e['rtp']-base)%2**32 for e in entries)==[i*step for i in range(len(entries))]
 rates=[s for s in m['sr'] if s['ssrc']==entries[0]['ssrc']]
 epochs=[Fraction(s['seconds'])+Fraction(s['fraction'],2**32)+Fraction(((base-s['rtp']+2**31)%2**32)-2**31,rate) for s in rates]
 m[key+'_epochs']=[str(x) for x in epochs]
 print(key,'sr',len(rates),'epoch',float(epochs[0]),'epoch_range_us',float((max(epochs)-min(epochs))*1000000))
b=[Fraction(x)-Fraction(y)-Fraction(shift,44100) for x in m['video_epochs'] for y in m['audio_epochs']]
delta=json.loads((p/'composition-r452-content-result.json').read_text())[0]['video_minus_audio_ms']
res=dict(input_audio_exact_reference_after_verified_skip=True,exact_equal_samples=len(a)-shift,verified_skip_samples=shift,raw_input_minus_reference_samples=len(a)-len(r),reference_unmatched_tail_samples=len(r)-(len(a)-shift),input_video_frames=len(m['video']),input_audio_units=len(m['audio']),video_epoch=m['video_epochs'],audio_epoch=m['audio_epochs'],input_bias_min_ms=float(min(b)*1000),input_bias_max_ms=float(max(b)*1000),output_candidate_ms=delta,residual_min_ms=delta-float(max(b)*1000),residual_max_ms=delta-float(min(b)*1000),fragment_packets=m['fragment_packets'])
print(json.dumps(res,indent=2))
(p/'composition-r452-attribution.json').write_text(json.dumps(res,indent=2))

```

```python
import pathlib,json,numpy as np
from scipy.signal import correlate,find_peaks
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
r=np.fromfile(p/'composition-r452-reference.f32',dtype='<f4').astype(np.float64);o=np.fromfile(p/'composition-r452-received.f32',dtype='<f4').astype(np.float64)
fs=44100;k=fs;n=110*fs;w=o[k:k+n];energy=np.r_[0,np.cumsum(r*r)];c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum((energy[n:]-energy[:-n])*np.dot(w,w),1e-300))
j=int(np.argmax(c));peaks,_=find_peaks(c,distance=fs);best=sorted(peaks,key=lambda q:c[q],reverse=True)[:5]
res=dict(output_start_pts=1,window_seconds=110,source_start_pts=j/fs,correlation=float(c[j]),candidate_lag_samples=j-k,other_peaks=[dict(source_start_pts=float(q/fs),correlation=float(c[q])) for q in best],valid_reference_start_range_seconds=[0,(len(r)-n)/fs])
print(json.dumps(res,indent=2));(p/'composition-r452-long-audio-match.json').write_text(json.dumps(res,indent=2))

```

输出TS恢复复用r451已归档提取程序，将前缀r451替换r452，只取dst62024。输出视频选帧复用r451命令保留前六点，移除4500/5400/6300/7200；其余参数不变。

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -y -r 30 -f h264 -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-input.h264' -an -vf "select='eq(n,0)+eq(n,300)+eq(n,900)',crop=640:70:0:0,showinfo" -fps_mode passthrough 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-input-marker-%02d.png' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r452-input-video.log'
```

首包事实命令：`D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -read_intervals '%+#3' -show_packets -show_entries packet=pts,pts_time,duration,side_data_list -of json D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4`。

## 原始证据摘要

attribution.json
{
  "input_audio_exact_reference_after_verified_skip": true,
  "exact_equal_samples": 5291008,
  "verified_skip_samples": 1024,
  "raw_input_minus_reference_samples": 32,
  "reference_unmatched_tail_samples": 992,
  "input_video_frames": 3600,
  "input_audio_units": 5168,
  "video_epoch": [
    "17196243443256301453/4294967296",
    "2149530430407037681663/536870912000",
    "134345651900439855097/33554432000",
    "2149530430407037681641/536870912000",
    "1074765215203518840809/268435456000",
    "2149530430407037681601/536870912000",
    "1074765215203518840831/268435456000",
    "17196243443256301453/4294967296",
    "2149530430407037681639/536870912000",
    "1074765215203518840771/268435456000",
    "2149530430407037681617/536870912000",
    "268691303800879710201/67108864000",
    "1343456519004398551/335544320",
    "2149530430407037681643/536870912000",
    "537382607601759420411/134217728000",
    "214953043040703768157/53687091200",
    "2001906214661/500",
    "2149530430407037681577/536870912000",
    "107476521520351884077/26843545600",
    "1074765215203518840773/268435456000",
    "2149530430407037681639/536870912000",
    "2149530430407037681649/536870912000",
    "537382607601759420403/134217728000",
    "537382607601759420401/134217728000",
    "2149530430407037681633/536870912000"
  ],
  "audio_epoch": [
    "189588583950914519295529/47352014438400",
    "94794291975457581771767/23676007219200",
    "189588583950915163544857/47352014438400",
    "2962321624233051108059/739875225600",
    "94794291975457581772649/23676007219200",
    "7583543358036580771927/1894080577536",
    "63196194650305126095859/15784004812800",
    "63196194650305126096153/15784004812800",
    "63196194650305126096447/15784004812800",
    "31598097325152455673703/7892002406400",
    "94794291975457420712081/23676007219200",
    "47397145987728737199101/11838003609600",
    "94794291975457474398643/23676007219200",
    "94794291975457581770003/23676007219200",
    "189588583950915163540447/47352014438400",
    "189588583950914626675709/47352014438400",
    "47397145987728710354497/11838003609600",
    "189588583950915270913571/47352014438400",
    "189588583950914519295529/47352014438400",
    "189588583950915485667757/47352014438400",
    "12639238930060982269981/3156800962560",
    "7583543358036580771927/1894080577536",
    "23698572993864314912327/5919001804800",
    "31598097325152563048003/7892002406400",
    "189588583950914519292883/47352014438400"
  ],
  "input_bias_min_ms": 208.77097486856218,
  "input_bias_max_ms": 208.79138341553238,
  "output_candidate_ms": 225.80498866213094,
  "residual_min_ms": 17.01360524659856,
  "residual_max_ms": 17.03401379356876,
  "fragment_packets": 122
}
content-result.json
[
  {
    "output_pts": 1,
    "video_frame": 162,
    "source_video_pts": 5.4,
    "source_audio_pts": 5.625804988662131,
    "audio_offset": -4.625804988662131,
    "video_offset": -4.4,
    "video_minus_audio_ms": 225.80498866213094,
    "correlation": 0.9981625210195791,
    "other_peaks": [
      {
        "source_time": 5.625804988662131,
        "correlation": 0.9981625210195791
      },
      {
        "source_time": 88.2924716553288,
        "correlation": 0.9959166520458113
      },
      {
        "source_time": 104.82580498866213,
        "correlation": 0.995620547378637
      },
      {
        "source_time": 38.692471655328795,
        "correlation": 0.9954220809439523
      },
      {
        "source_time": 22.159138321995464,
        "correlation": 0.9947434983239681
      }
    ]
  },
  {
    "output_pts": 10,
    "video_frame": 432,
    "source_video_pts": 14.4,
    "source_audio_pts": 14.625804988662132,
    "audio_offset": -4.625804988662132,
    "video_offset": -4.4,
    "video_minus_audio_ms": 225.80498866213185,
    "correlation": 0.9993897895047357,
    "other_peaks": [
      {
        "source_time": 14.625804988662132,
        "correlation": 0.9993897895047357
      },
      {
        "source_time": 47.692471655328795,
        "correlation": 0.9987323775903201
      },
      {
        "source_time": 64.22580498866213,
        "correlation": 0.9987211706904376
      },
      {
        "source_time": 113.82580498866213,
        "correlation": 0.9987149017607825
      },
      {
        "source_time": 97.2924716553288,
        "correlation": 0.9987049584721003
      }
    ]
  },
  {
    "output_pts": 30,
    "video_frame": 1032,
    "source_video_pts": 34.4,
    "source_audio_pts": 34.62580498866213,
    "audio_offset": -4.625804988662132,
    "video_offset": -4.399999999999999,
    "video_minus_audio_ms": 225.8049886621336,
    "correlation": 0.9995490866684091,
    "other_peaks": [
      {
        "source_time": 34.62580498866213,
        "correlation": 0.9995490866684091
      },
      {
        "source_time": 84.22580498866213,
        "correlation": 0.9994351408016318
      },
      {
        "source_time": 100.75913832199547,
        "correlation": 0.9994282559004747
      },
      {
        "source_time": 18.092471655328797,
        "correlation": 0.9994248252896222
      },
      {
        "source_time": 67.6924716553288,
        "correlation": 0.9994210391301618
      }
    ]
  },
  {
    "output_pts": 60,
    "video_frame": 1932,
    "source_video_pts": 64.4,
    "source_audio_pts": 64.62580498866213,
    "audio_offset": -4.625804988662125,
    "video_offset": -4.400000000000006,
    "video_minus_audio_ms": 225.8049886621194,
    "correlation": 0.9985037138080961,
    "other_peaks": [
      {
        "source_time": 64.62580498866213,
        "correlation": 0.9985037138080961
      },
      {
        "source_time": 48.0924716553288,
        "correlation": 0.9973348561448289
      },
      {
        "source_time": 114.22580498866213,
        "correlation": 0.9973013185158286
      },
      {
        "source_time": 31.559138321995466,
        "correlation": 0.9972631004257927
      },
      {
        "source_time": 81.15913832199547,
        "correlation": 0.9972172144908117
      }
    ]
  },
  {
    "output_pts": 90,
    "video_frame": 2832,
    "source_video_pts": 94.4,
    "source_audio_pts": 94.62580498866213,
    "audio_offset": -4.625804988662125,
    "video_offset": -4.400000000000006,
    "video_minus_audio_ms": 225.8049886621194,
    "correlation": 0.9991614640886228,
    "other_peaks": [
      {
        "source_time": 94.62580498866213,
        "correlation": 0.9991614640886228
      },
      {
        "source_time": 111.15913832199547,
        "correlation": 0.9991044693595033
      },
      {
        "source_time": 28.4924716553288,
        "correlation": 0.9990405932252439
      },
      {
        "source_time": 61.55913832199546,
        "correlation": 0.9989290557907662
      },
      {
        "source_time": 78.0924716553288,
        "correlation": 0.9988392365519598
      }
    ]
  },
  {
    "output_pts": 105,
    "video_frame": 3282,
    "source_video_pts": 109.4,
    "source_audio_pts": 109.62580498866213,
    "audio_offset": -4.625804988662125,
    "video_offset": -4.400000000000006,
    "video_minus_audio_ms": 225.8049886621194,
    "correlation": 0.999313312664206,
    "other_peaks": [
      {
        "source_time": 109.62580498866213,
        "correlation": 0.999313312664206
      },
      {
        "source_time": 76.55913832199546,
        "correlation": 0.99566817853445
      },
      {
        "source_time": 10.425804988662131,
        "correlation": 0.9945714408328534
      },
      {
        "source_time": 26.959138321995464,
        "correlation": 0.9945504082778527
      },
      {
        "source_time": 43.4924716553288,
        "correlation": 0.9928176852780247
      }
    ]
  }
]
long-audio-match.json
{
  "output_start_pts": 1,
  "window_seconds": 110,
  "source_start_pts": 5.625804988662131,
  "correlation": 0.9987034040111639,
  "candidate_lag_samples": 203998,
  "other_peaks": [
    {
      "source_start_pts": 5.625804988662131,
      "correlation": 0.9987034040111639
    },
    {
      "source_start_pts": 4.305850340136055,
      "correlation": 0.07047492521752113
    },
    {
      "source_start_pts": 6.945759637188209,
      "correlation": 0.06965559344801851
    },
    {
      "source_start_pts": 3.2881179138321994,
      "correlation": 0.049728723477296274
    },
    {
      "source_start_pts": 7.963492063492064,
      "correlation": 0.048163412162070145
    }
  ],
  "valid_reference_start_range_seconds": [
    0,
    10.0
  ]
}
wire.json
{
  "datagrams": 90557,
  "rtp_gaps": 0,
  "ts_packets": 566648,
  "tei": 0,
  "cc_errors": 0,
  "stderr": "",
  "ts_bytes": 106529824
}
cli-error.log
[in @ 00000212647b2e40] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000212647b2e40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45107.760489
[in @ 00000212647b3040] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000212647b3040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20040.443678
[in @ 0000021264bb8d00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021264bb8d00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8820.574533
[in @ 000002126f5c8600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002126f5c8600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29342.946567
[h264 @ 000002126ac14a00] co located POCs unavailable
[h264 @ 000002126ac14a00] co located POCs unavailable
[hevc @ 000002126ac18140] Could not find ref with POC 52
[hevc @ 000002126ac18140] Error constructing the frame RPS.
[hevc @ 000002126ac18140] Skipping invalid undecodable NALU: 0
[hevc @ 000002126ac18140] Could not find ref with POC 54
[hevc @ 000002126ac18140] Error constructing the frame RPS.
[hevc @ 000002126ac18140] Skipping invalid undecodable NALU: 0
[hevc @ 000002126ac18140] Could not find ref with POC 56
[hevc @ 000002126ac18140] Error constructing the frame RPS.
[hevc @ 000002126ac18140] Skipping invalid undecodable NALU: 1
[hevc @ 000002126ac18140] Could not find ref with POC 56
[hevc @ 000002126ac18140] Error constructing the frame RPS.
[hevc @ 000002126ac18140] Skipping invalid undecodable NALU: 1
[hevc @ 000002126ac18140] Could not find ref with POC 56
[hevc @ 000002126ac18140] Error constructing the frame RPS.
[hevc @ 000002126ac18140] Skipping invalid undecodable NALU: 0
[in @ 000002125b73a600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002125b73a600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29342.946567
[in @ 000002125fa7d180] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002125fa7b700] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002125fa7d180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45107.760489
[in @ 000002125fa7d380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002125fa7b700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8820.974533
[in @ 000002125fa7d380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20040.443678
[aac @ 000002126ac17280] Qavg: 14255.868

[2026-11-16 18:07:46.301] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=1207 audio_canonical_dispatch_ns=28003265306 video_canonical_lower_bound_ns=28033333333 observed_after_output_commit=1
[2026-11-16 18:09:38.127] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=15115 committed_datagrams=90591 committed_payload_bytes=107618828 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=36 ordered_queue_maximum_wire_bytes=47688 pacing_reserved=90591 pacing_cancelled=0 pacing_submitted=90591 pacing_maximum_submit_lateness_ns=6662600 pacing_worst_late_global_sequence=1 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=36 backlog_high_water_wire_bytes=47688 backlog_max_residence_ns=40530800 last_materialized_sequence=90591 last_scheduled_sequence=90591 last_submitted_sequence=90591 last_committed_sequence=90591 submitted_service_generation=1 submitted_service_sequence=90591 submitted_service_completed_ns=144880842700 submitted_service_duration_ns=16640 submitted_service_available_ns=144880859340 submitted_scope_available_ns=144880859340 shared_scope_charged_datagrams=90591 shared_scope_charged_wire_bytes=110155376 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554646 endpoint_1_target_effective_socket_bytes=277323 endpoint_1_api_requested_socket_bytes=277323 endpoint_1_effective_socket_bytes=277323 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=90557 endpoint_1_committed_payload_bytes=107616508 endpoint_2_target_effective_socket_bytes=277323 endpoint_2_api_requested_socket_bytes=277323 endpoint_2_effective_socket_bytes=277323 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=34 endpoint_2_committed_payload_bytes=2320
[CLI] final runtime report: queued=0, peakQueued=1778, threads=102, processThreads=170, workers=0, workerProgress=341750, workerProcessCalls=2932821, workerWaits=2591052, workerWakeups=2580361, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=398, logicalProcessors=22, averageSystemMachineCpuPercent=27.053160, averageProcessMachineCpuPercent=3.206986, peakProcessMachineCpuPercent=10.079051, averageProcessSingleCoreCpuPercent=70.553684, peakProcessSingleCoreCpuPercent=221.739130, initialWorkingSetBytes=229339136, workingSetBytes=277430272, peakWorkingSetBytes=295514112, totalPushed=310489, totalPopped=310164, droppedBuffers=0, encodedPacketsPushed=46896, encodedPacketsPopped=46896, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=19221198, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1353, graphPayloadReservations=214020, graphPayloadReleases=214020, graphPayloadPressureFailures=0, lastInputReceivedAtNs=34016607536200
### 实际SR记录
[
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812429,
    "fraction": 966367641,
    "rtp": 319635265
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812429,
    "fraction": 2456721293,
    "rtp": 793514208
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812434,
    "fraction": 1035087118,
    "rtp": 319856470
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812434,
    "fraction": 2568390443,
    "rtp": 793966548
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812439,
    "fraction": 1163936137,
    "rtp": 320078293
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812439,
    "fraction": 2581275344,
    "rtp": 794416818
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812444,
    "fraction": 1168231104,
    "rtp": 320298837
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812444,
    "fraction": 2701534429,
    "rtp": 794869338
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812449,
    "fraction": 1206885810,
    "rtp": 320519734
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812449,
    "fraction": 2718714298,
    "rtp": 795319698
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812454,
    "fraction": 1224065679,
    "rtp": 320740411
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812454,
    "fraction": 2894807957,
    "rtp": 795773388
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812459,
    "fraction": 1258425417,
    "rtp": 320961263
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812459,
    "fraction": 2989297238,
    "rtp": 796225368
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812464,
    "fraction": 1344324763,
    "rtp": 321182645
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812464,
    "fraction": 2993592205,
    "rtp": 796675458
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812469,
    "fraction": 1430224109,
    "rtp": 321404027
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812469,
    "fraction": 3006477107,
    "rtp": 797125728
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812474,
    "fraction": 1447403978,
    "rtp": 321624704
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812474,
    "fraction": 3032246910,
    "rtp": 797576268
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812479,
    "fraction": 1451698946,
    "rtp": 321845248
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812479,
    "fraction": 3139621093,
    "rtp": 798028518
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812484,
    "fraction": 1584842932,
    "rtp": 322067115
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812484,
    "fraction": 3169685864,
    "rtp": 798479148
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812489,
    "fraction": 1670742278,
    "rtp": 322288497
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812489,
    "fraction": 3315714752,
    "rtp": 798932208
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812494,
    "fraction": 1765231558,
    "rtp": 322509967
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812494,
    "fraction": 3470333575,
    "rtp": 799385448
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812499,
    "fraction": 1808181231,
    "rtp": 322730908
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812499,
    "fraction": 3586297692,
    "rtp": 799837878
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812504,
    "fraction": 1872605741,
    "rtp": 322952070
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812504,
    "fraction": 3594887626,
    "rtp": 800288058
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812509,
    "fraction": 1924145348,
    "rtp": 323173099
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812509,
    "fraction": 3758096384,
    "rtp": 800741478
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812514,
    "fraction": 2027224563,
    "rtp": 323394657
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812514,
    "fraction": 3869765533,
    "rtp": 801193818
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812519,
    "fraction": 2040109465,
    "rtp": 323615290
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812519,
    "fraction": 3874060500,
    "rtp": 801643908
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812524,
    "fraction": 2164663517,
    "rtp": 323837068
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812524,
    "fraction": 4032974290,
    "rtp": 802097238
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812529,
    "fraction": 2177548419,
    "rtp": 324057701
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812529,
    "fraction": 4080218931,
    "rtp": 802548228
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812534,
    "fraction": 2297807503,
    "rtp": 324279436
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812534,
    "fraction": 4166118277,
    "rtp": 803000028
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812539,
    "fraction": 2340757176,
    "rtp": 324500377
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812539,
    "fraction": 4170413244,
    "rtp": 803450118
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812544,
    "fraction": 2375116914,
    "rtp": 324721229
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812545,
    "fraction": 21474836,
    "rtp": 803903178
  },
  {
    "port": 61021,
    "ssrc": 2155566265,
    "seconds": 4003812548,
    "fraction": 2847563317,
    "rtp": 804232398
  },
  {
    "port": 61023,
    "ssrc": 4185778127,
    "seconds": 4003812548,
    "fraction": 2856153251,
    "rtp": 324902569
  }
]
### 清理清单
composition-r452-attribution.json
composition-r452-audio-map.json
composition-r452-capture.log
composition-r452-capture.out
composition-r452-cli-error.log
composition-r452-cli.log
composition-r452-content-result.json
composition-r452-input-audio.log
composition-r452-input-map.json
composition-r452-input-marker-01.png
composition-r452-input-marker-02.png
composition-r452-input-marker-03.png
composition-r452-input-video.log
composition-r452-input.aac
composition-r452-input.f32
composition-r452-input.h264
composition-r452-long-audio-match.json
composition-r452-marker-01.png
composition-r452-marker-02.png
composition-r452-marker-03.png
composition-r452-marker-04.png
composition-r452-marker-05.png
composition-r452-marker-06.png
composition-r452-native-2026-11-16-18h07m49s419.png
composition-r452-native-2026-11-16-18h09m30s741.png
composition-r452-output.pcapng
composition-r452-output.sdp
composition-r452-output.ts
composition-r452-received-audio.log
composition-r452-received.f32
composition-r452-reference-audio.log
composition-r452-reference.f32
composition-r452-source0.log
composition-r452-source0.out
composition-r452-source1.log
composition-r452-source1.out
composition-r452-source2.log
composition-r452-source2.out
composition-r452-source3.log
composition-r452-source3.out
composition-r452-video-analysis.log
composition-r452-vlc.log
composition-r452-wire.json

独立复核PASS：原PCM110秒整段相关独立重算一致，可作为本轮有限内容对应/SR基准归因。17.014–17.034ms范围来自SR映射变化，不是完整统计置信区间；110秒整体相关不证明逐时刻恒定，六点不代替首次有效输出/全程p95p99/重入/长稳。

清理完成：上述43项临时产物逐项删除，本轮7个精确PID已确认无残留，指定120秒源和正式产物保留。
