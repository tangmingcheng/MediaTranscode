# CUDA canvas复制stream隔离（r236）

## 失败证据与适用依据

r235同次canvas134.1904ms，其中黑底copy117.3218ms，黑底source stream同步91.8757ms；尚未证明它直接导致seq168644超期。[NVIDIA同步语义](https://docs.nvidia.com/cuda/cuda-driver-api/stream-sync-behavior.html)明确non-blocking stream不参与legacy stream隐式同步；[stream管理](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__STREAM.html)规定destroy不是完成屏障。故采用私有non-blocking stream并显式保留依赖/完成，不删除所有同步。

## 实施契约

- 保持现有共享DAG、canvas producer、同步adapter接口及单owner线程；无新增线程、队列、公开参数、媒体拷贝层或外部FFmpeg修改。
- 内部copy调用显式携带source readiness：黑底prepare完成upload及readback验证后，producer在只读生命周期内可证明Completed；动态tile仍为DeviceStream，保留源stream同步。RGA仍使用IM_SYNC；无需引入CUDA机制，平台差异仅在adapter。
- CUDA准备阶段创建私有non-blocking stream并核验flags；失败直接返回，不回退默认stream。不改AVCUDADeviceContext::stream。既有prepare执行黑底upload/download和复制，以及动态tile复制验证，失败不能进入运行DAG。
- 设备复制只提交到私有stream；host upload/download保留原同步API。每次操作及部分提交失败后等待已提交工作完成，方可释放/发布帧。资源RAII持有context引用所需frames、驱动库与stream；context push/pop也成对管理，清理错误可观测且不覆盖首个错误。
- 固定一个stream/adapter，既有surface/header预算不变；生产不新增像素缓存。初始化日志记录实际设备stream、私有stream flags；动态源stream另记录首次事实。
- 私有stream不是执行时限保证；动态源stream等待、GPU执行、驱动和OS调度仍可能超预算。Windows完整验收通过后才能进入RKMPP。

## 本机能力探测（2026-10-16）

独立context的最小Driver API能力探测全部返回0，flags回读1（CU_STREAM_NON_BLOCKING），4字节设备异步复制回读0x12345678，内存/stream/context均释放。它不替代生产context/FFmpeg运行值或原规格实流验收，无临时文件。

```powershell
& 'D:/Python/python.exe' -c 'import ctypes as c
d=c.WinDLL("C:/Windows/System32/nvcuda.dll")
def call(name,args,types):
 f=getattr(d,name); f.argtypes=types; f.restype=c.c_int
 rc=f(*args); print(name,rc,flush=True)
 if rc: raise RuntimeError((name,rc))
p=c.c_void_p; u=c.c_uint; z=c.c_size_t; q=c.c_uint64
ctx=p(); stream=p(); a=q(); b=q()
call("cuInit",[0],[u])
call("cuCtxCreate_v2",[c.byref(ctx),0,0],[c.POINTER(p),u,c.c_int])
try:
 call("cuStreamCreate",[c.byref(stream),1],[c.POINTER(p),u])
 try:
  flags=u();call("cuStreamGetFlags",[stream,c.byref(flags)],[p,c.POINTER(u)]);print("stream_flags",flags.value,flush=True)
  if flags.value!=1: raise RuntimeError("nonblocking flag mismatch")
  call("cuMemAlloc_v2",[c.byref(a),4],[c.POINTER(q),z])
  try:
   call("cuMemAlloc_v2",[c.byref(b),4],[c.POINTER(q),z])
   try:
    value=u(0x12345678);actual=u()
    call("cuMemcpyHtoD_v2",[a,c.byref(value),4],[q,p,z])
    call("cuMemcpyDtoDAsync_v2",[b,a,4,stream],[q,q,z,p])
    call("cuStreamSynchronize",[stream],[p])
    call("cuMemcpyDtoH_v2",[c.byref(actual),b,4],[p,q,z])
    print("copy_readback",hex(actual.value),flush=True)
    if actual.value!=value.value: raise RuntimeError("readback mismatch")
   finally: call("cuMemFree_v2",[b],[q])
  finally: call("cuMemFree_v2",[a],[q])
 finally: call("cuStreamDestroy_v2",[stream],[p])
finally: call("cuCtxDestroy_v2",[ctx],[p])'
```

## r236 实施与审查进度

已实现私有 non-blocking stream、内部 readiness、完成屏障与 RAII。host 复制使用普通 cuMemcpy2D_v2，完成屏障明确等待默认 stream（nullptr），不等待私有 stream；依据 NVIDIA API Synchronization behavior，pageable host 上传返回时 DMA 可能尚未完成。首个错误保持原样返回，后续 completion/context restore 错误单独记录。

首轮 Release clean-first/all-target 构建失败（会话77759，exit1）：ffnvcodec 最小头未提供 CUDA_ERROR_INVALID_CONTEXT。现已改用该头已有 CUDA_ERROR_INVALID_VALUE 表达 context pop 对象不匹配；第二轮全量构建正在执行（会话82649）。没有修改外部依赖。独立审查B复审限定PASS；实流尚未验证，不宣称修复超期。

第二轮全量构建会话82649已exit0，configure/build均0，718目标完成。CMakeCache/build.ninja及两CLI产物实际存在；未运行local CLI。realtime CLI为6053376字节，2026-10-16 10:43:44，SHA256=E5DCE25592C2B8FDA3C3B3F3C3B6E0ED7C64F1238DB8AFD5CD105C0C59962289。审查A、B均对修订后的源码明确限定PASS，git diff --check通过。仍须运行原规格realtime实流与VLC原生快照；没有本轮运行时通过结论。
