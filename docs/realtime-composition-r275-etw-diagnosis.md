# r275：Windows等待诊断能力核验

状态：ETW采集未启动，未运行媒体测试；无代码或外部FFmpeg修改。

r274已由代码和同帧时间线确认canvas后紧接avcodec_send_frame长调用。现有GetThreadTimes只提供累计CPU时间，不能区分等待与Ready排队。微软CPU精确等待分析以CSwitch/ReadyThread区分这两者：https://learn.microsoft.com/en-us/windows-hardware/test/wpt/optimizing-performance-and-responsiveness-exercise-3 。

本机FFmpeg nvenc.c的ff_nvenc_receive_packet调用nvenc_send_frame后还可能process_output_surface；后者涉及nvEncLockBitstream。不能把整个avcodec_send_frame耗时等同nvEncEncodePicture，不能由源事件Ready推断GPU空闲。

## 实际诊断命令与结果

```powershell
& 'C:/Windows/System32/wpr.exe' -status
& 'C:/Windows/System32/wpr.exe' -profiles
& 'C:/Windows/System32/wpr.exe' -profiledetails CPU
& 'C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe' -on PROC_THREAD+LOADER+CSWITCH+DISPATCHER+PROFILE -stackwalk CSwitch+ReadyThread+Profile -buffersize 1024 -minbuffers 64 -maxbuffers 256 -f 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r275-kernel.etl' -FileMode Circular -MaxFile 512
& 'C:/Windows/System32/wpr.exe' -start CPU -filemode -recordtempto 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r275-wpr'
& 'C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe' -on PROC_THREAD+LOADER+CSWITCH+DISPATCHER -stackwalk CSwitch+ReadyThread -buffersize 1024 -minbuffers 64 -maxbuffers 256 -f 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r275-kernel.etl' -FileMode Circular -MaxFile 512
```

初始WPR not recording，无NT Kernel Logger。本机提供CPU profile和CSwitch/ReadyThread栈。首个xperf返回0x3ec；WPR返回0xc5585011，原文“Failed to enable the policy to profile system performance.”；去掉PROFILE后xperf返回0x5（Access denied）。不修改系统策略、不停止其他系统logger。结束时WPR仍not recording，没有生成ETL或临时文件；仅创建的空项目内目录已删除。

该权限限制阻塞此采集方式，不阻塞其余开发。继续完成writer/metadata存储预算及取消语义；保持时序问题未解决，不以无观测替代验收。
