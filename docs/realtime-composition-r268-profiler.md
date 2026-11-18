# r268：本机 CUDA 跟踪能力核验

## 真实结果

本机GPU：NVIDIA GeForce RTX4060 Laptop GPU，驱动610.62，WDDM。
已安装Nsight Systems2024.6.2.225；不在PATH，但绝对路径可运行。未安装或修改外部工具。

环境自检退出0，但文本明确：Administrator privileges: No；Sampling Environment: Fail。不能把退出0当采样通过。

实际帮助命令探测退出0并产生44777B报告。工具原文：
`WARNING: Wddm trace requires administrative privileges, disabling.`
因此当前无WDDM采集能力；关闭CPU采样后profile启动可用。目标仅运行realtime CLI --help，没有媒体/CUDA工作，不能证明真实CUDA事件已采集，也不是验收。

```powershell
& 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' status --environment
& 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' profile --trace=cuda,wddm --sample=none --cpuctxsw=none --kill=false --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-profiler-probe 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --help
```

依据：https://docs.nvidia.com/nsight-systems/UserGuide/index.html 与本机profile --help。已安装版本kill选项是false，不套用新版none写法。--kill=false避免采集结束强杀目标。

## 下一真实诊断

保留r267全部源参数及失源/重入过程，profile仅启用cuda，sample/cpuctxsw均none，目标仍是生产realtime CLI；VLC继续播放与原生快照。监控实际CLI子PID，不能把nsys的CPU/内存记成CLI。记录collector与CLI各自退出码。GPU/API时间线用于定位长等待，不把插桩运行代替最终无插桩验收。

本轮报告composition-r268-profiler-probe.nsys-rep在记录后删除；工具打印的临时C:/Users/ROG/AppData/Local/Temp/nsys-report-bbea.qdstrm已不存在，无需删除。没有启动真实源或VLC。
