# r250：生产端帧就绪事实设计（未实施）

## 触发证据
r249 同r248产物复验再次失败，protocol materialization 晚于deadline19.843536ms；最大canvas137.3582ms中源stream等待135.6477ms、批完成1.4153ms。缺少该wait与失败音频的完整同帧因果链，不能直接等同。批处理保留，但不再声称它足以修复迟到。

## 依据与适用边界
[NVIDIA Event API](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html)的record捕获调用时已提交工作，后续stream提交不扩展事件；未record事件是空工作，不能当就绪证据。[Stream API](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__STREAM.html)的wait event用于设备端依赖。采用生产端事件随frame owner传递，不能在canvas现场record冒充逐帧完成。默认stream已有隐式依赖仍在，不承诺执行时限。

## 生产者和所有权矩阵
- FFmpegFrameBuffer持有实际像素FramePtr，拟同层持有内部readiness lease；canonical/display wrapper通过media共享，不复制事件。FFmpegFrameView沿已有递归路径获取。
- runtime filter：成功sink frame后、包装发布前capture。scale_cuda使用hwctx stream已有本机源码证据。
- runtime直接decode及prepare first-frame probe：必须核验实际decoder生产stream；不能从AVHWFramesContext存在直接推断。
- passthrough/alias/clone/takeFrame：必须保持frame与readiness同寿命，不能丢失lease或直接标Completed。
- hardware transfer：由实际adapter的同步合同或生产stream形成事实。
- prepare filter/tile：与runtime共用capture adapter；CompositionPreparer当前裸tile列表须同时保有readiness。
- black与canvas：已成功同步upload/batch可标Completed。RGA继续真实同步事实。
不占用已有codec lineage的opaque_ref。

## 线程、容量和失败合同
仍复用现有DAG owner线程，无额外媒体队列。初版拟由planner按同时持有frame owner上界推导event pool（已被下文简化方案取代），覆盖pending、队列、startup、held/future与prepared；别名共享lease。准备时建立固定池并做真实capability probe。池槽/控制块纳入现有allocator；driver内部event内存无权威字节值，不纳入“已精确计量”的宣称。
CUDA batch在每个tile copy前wait该event，最后仍完成批次后才返回。record/wait/complete失败不发布目标，保留首错；purge/析构保证frame/event/context寿命。缺生产stream或容量事实在构图前失败，无runtime fallback。

## 下一步
先核验decode/transfer能力及frame移交边界，形成可审查的内部契约，再实施并双独立审查。尚未新增类型/参数/代码，没有扩大外部FFmpeg范围。四源HEVC非cadence、内容A/V/静音、C API/ABI及Windows后RKMPP仍属于完整目标；事件方案不能替代这些缺项。

## r250 补充核验
本机 native safe-output 链：nvdec.c:568–571调用av_frame_make_writable；frame.c:552–578从同hw_frames_ctx分配tmp并copy，:677硬件copy转av_hwframe_transfer_data；hwcontext.c:481优先source transfer_from；hwcontext_cuda.c:281提交到该ctx的device stream。cuvid CUDA输出在cuviddec.c:647同样提交到device stream。这里是本机源码条件证据，不证明r249实际decoder名称；下次须保留实际decoder readback。

runtime帧数量可来自CompositionResourcePlanner:116–134对应Video/Frame生产者的reachable+additional owner额度。Decoder/Filter在receive前reserve并随输出attach。准备tile在CompositionPreparer:313–323 canvas准备后释放，再prepareReplay；不能借用runtime额度。动态输出reserveRetentionGrowth可增加帧配额，退役时已有对象也可能超过缩小后的瞬时quota。

### 设计修订：不强制固定事件池
独立审查确认固定池是实现选择，不是正确性必需。优先每个已准入帧owner至多一个事件，metadata经实际计费PMR；别名共享owner，不增加媒体链路或公开参数。事件完成并处理销毁后才归还credit，避免CUDA异步destroy仍留driver资源时继续循环创建。数量边界以准入及退役中credit计，不宣称总存量始终小于瞬时quota。每帧create/destroy的尾延迟需实流验证；若后续采用池化，再形成独立且充分容量依据。准备期owner/metadata预算仍需闭合。

[GStreamer CUDA memory](https://gstreamer.freedesktop.org/documentation/cuda/gstcudamemory.html)将context/stream及同步责任与memory关联；其实现不能直接当成本项目逐帧event已经等价的证明。此处复用所有权关联原则及NVIDIA事件API语义。

### 本机能力探测（非生产链路验收）
两个私有non-blocking stream、disable-timing event，producer写值后record，consumer wait event再复制，完成后query和readback均成功。全部API返回0，回读0x12345678；显式完成、释放内存、事件、stream和context均成功。无文件/进程残留。该独立context探测不证明生产decoder/context/alias及真实时限。
```powershell
& 'D:/Python/python.exe' -c 'import ctypes as c
d=c.WinDLL("C:/Windows/System32/nvcuda.dll")
p=c.c_void_p; u=c.c_uint; q=c.c_uint64; z=c.c_size_t
def call(name,args,types):
 f=getattr(d,name); f.argtypes=types; f.restype=c.c_int
 rc=f(*args); print(name,rc,flush=True)
 if rc: raise RuntimeError((name,rc))
ctx=p(); producer=p(); consumer=p(); event=p(); src=q(); dst=q()
call("cuInit",[0],[u])
call("cuCtxCreate_v2",[c.byref(ctx),0,0],[c.POINTER(p),u,c.c_int])
try:
 call("cuStreamCreate",[c.byref(producer),1],[c.POINTER(p),u])
 try:
  call("cuStreamCreate",[c.byref(consumer),1],[c.POINTER(p),u])
  try:
   call("cuEventCreate",[c.byref(event),2],[c.POINTER(p),u])
   try:
    call("cuMemAlloc_v2",[c.byref(src),4],[c.POINTER(q),z])
    try:
     call("cuMemAlloc_v2",[c.byref(dst),4],[c.POINTER(q),z])
     try:
      call("cuMemsetD32Async",[src,0x12345678,1,producer],[q,u,z,p])
      call("cuEventRecord",[event,producer],[p,p])
      call("cuStreamWaitEvent",[consumer,event,0],[p,p,u])
      call("cuMemcpyDtoDAsync_v2",[dst,src,4,consumer],[q,q,z,p])
      call("cuStreamSynchronize",[consumer],[p])
      call("cuEventQuery",[event],[p])
      value=u()
      call("cuMemcpyDtoH_v2",[c.byref(value),dst,4],[p,q,z])
      print("readback",hex(value.value),flush=True)
      if value.value!=0x12345678: raise RuntimeError("readback mismatch")
     finally:
      call("cuStreamSynchronize",[consumer],[p])
      call("cuMemFree_v2",[dst],[q])
    finally:
     call("cuStreamSynchronize",[producer],[p])
     call("cuMemFree_v2",[src],[q])
   finally:
    call("cuEventSynchronize",[event],[p])
    call("cuEventDestroy_v2",[event],[p])
  finally: call("cuStreamDestroy_v2",[consumer],[p])
 finally: call("cuStreamDestroy_v2",[producer],[p])
finally: call("cuCtxDestroy_v2",[ctx],[p])'
```
结果：
```text
cuInit 0
cuCtxCreate_v2 0
cuStreamCreate 0
cuStreamCreate 0
cuEventCreate 0
cuMemAlloc_v2 0
cuMemAlloc_v2 0
cuMemsetD32Async 0
cuEventRecord 0
cuStreamWaitEvent 0
cuMemcpyDtoDAsync_v2 0
cuStreamSynchronize 0
cuEventQuery 0
cuMemcpyDtoH_v2 0
readback 0x12345678
cuStreamSynchronize 0
cuMemFree_v2 0
cuStreamSynchronize 0
cuMemFree_v2 0
cuEventSynchronize 0
cuEventDestroy_v2 0
cuStreamDestroy_v2 0
cuStreamDestroy_v2 0
cuCtxDestroy_v2 0

```

## r250 前置实现：源元数据分配与时序解耦
三文件修改已冻结：Registrar对已有prepared RTP source绑定原videoReferenceResource；SourceAccessUnit/SourceDisplayPoint保留必须资源检查，cadence允许持有同一资源。保留唯一source owner、group存在、重复/启动后绑定禁止和缺RTP owner的display失败。没有新建resource，没有改startup或lineage选择。两位未参与实现者实际读取调用链后均限定PASS。
这只是event capture所需的共享预算接入基础，不声称事件已实现或metadata新增准入完成。Release全量构建会话78406运行中。

## 前置验证与下一所有权切片
全量构建78406退出0，r251实流三源与重入均120秒，四次VLC原生快照覆盖三格/全黑/重入/全黑，caller_stop退出0、errors0、payload0。实际native h264、源NULL CUDA stream已归档。不得将前置绑定当作超期修复，完整门禁未通过。
独立调查确认仅FFmpegFrameBuffer成员不足以覆盖codec内部持有，单独extended_buf token也不能保证跟随所有实际数据buf。下一切片复用MediaFfmpegPayloadOwnership的事务性allocation包装：每个实际buf wrapper释放原allocation前调用共享幂等完成；readiness不反向持有包装后buf，避免环；clone/takeFrame沿真实AVBufferRef持有，credit在完成与allocation之后释放。准备FramePtr也用同一函数，访问owner显式传递，不扫描未知av_buffer_get_opaque。wrapper及共享状态必须实际经PMR计费。该完整事件机制尚未实施。
