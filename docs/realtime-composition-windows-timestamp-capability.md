# Windows loopback 发送时间戳能力核验

2026-10-21：只读查询，未更改系统配置或生产源码。普通权限与提权执行结果一致。

- 目标127.0.0.1，GetBestInterfaceEx=0，接口index=1。
- ConvertInterfaceIndexToLuid=0，LUID=6755399441055744。
- 按已安装SDK 10.0.26100.0/iphlpapi.h定义构造结构，sizeof=24。
- GetInterfaceSupportedTimestampCapabilities=2。
- GetInterfaceActiveTimestampCapabilities=2。
- 返回错误：系统找不到指定的文件。失败后的全零输出结构不作为能力证据。

微软文档将成功定义为NO_ERROR，不支持为ERROR_NOT_SUPPORTED；本轮错误2不等价于明确不支持。当前适配器queryTimestampClockEvidence在此处返回空，因此不进入SIO_TIMESTAMPING。r290的source=0及tracked/observed=0与此一致；未声称已观察r290内部原始返回值。

依据：https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getinterfaceactivetimestampcapabilities 。下一步继续原规格真实CLI/VLC与loopback抓包的接收连续性验收；该证据不替代TX时间戳或远端交付保证。无需为此扩大外部FFmpeg范围或改动系统时间戳配置。

## 实际命令

~~~~powershell
& 'D:/Python/python.exe' -c @'
import ctypes as c, socket, json
class Address(c.Structure):
 _fields_=[('family',c.c_ushort),('port',c.c_ushort),('addr',c.c_ubyte*4),('zero',c.c_ubyte*8)]
class Caps(c.Structure):
 _fields_=[('frequency',c.c_ulonglong),('cross',c.c_ubyte),('hardware',c.c_ubyte*11),('software',c.c_ubyte*3)]
api=c.WinDLL('iphlpapi.dll')
addr=Address(); addr.family=socket.AF_INET; addr.addr[:]=socket.inet_aton('127.0.0.1')
idx=c.c_ulong(); luid=c.c_ulonglong()
r=api.GetBestInterfaceEx(c.byref(addr),c.byref(idx))
print(json.dumps({'route_result':r,'index':idx.value,'capability_struct_bytes':c.sizeof(Caps)}))
if r: raise SystemExit(1)
r=api.ConvertInterfaceIndexToLuid(idx,c.byref(luid))
print(json.dumps({'luid_result':r,'luid':luid.value}))
if r: raise SystemExit(1)
for name in ['GetInterfaceSupportedTimestampCapabilities','GetInterfaceActiveTimestampCapabilities']:
 cap=Caps(); result=getattr(api,name)(c.byref(luid),c.byref(cap))
 print(json.dumps({'api':name,'result':result,'message':c.FormatError(result),'frequency':cap.frequency,'hardware':list(cap.hardware),'software':list(cap.software)}))
'@
~~~~

未产生测试脚本、媒体进程或临时产物。
