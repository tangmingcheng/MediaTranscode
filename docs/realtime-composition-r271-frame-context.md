# r271：载荷快照保留硬件帧上下文

## 证据与范围

r270 在串行准入超期后清理崩溃，退出0xC0000005；实际avutil-60.dll RVA0x2753d对应cuda_buffer_free内访问context的指令。尚无异常栈证明具体释放者，不能声称已确认r270唯一根因。

项目MediaFfmpegPayloadReferences仅保留buf/extended_buf，却允许其生命周期超过原AVFrame；CUDA buffer free使用AVHWFramesContext作为opaque，因此缺少完整所有者保活。FFmpeg官方av_frame_ref同时引用hw_frames_ctx，av_frame_unref先释放buf/extended_buf再释放hw_frames_ctx；采用同一所有权顺序，不更改外部FFmpeg。

依据：https://ffmpeg.org/doxygen/trunk/frame_8c_source.html ，https://ffmpeg.org/doxygen/trunk/hwcontext__cuda_8c_source.html 。本机依赖源码libavutil/frame.c:341、496及hwcontext_cuda.c:96交叉核对。

## 实施与边界

snapshot增加独立RAII硬件frames引用，声明在像素引用容器之前，保证反向析构时所有像素先释放。创建时先引用frames上下文，失败即返回AllocationFailed；软件帧仍无该引用。共享实现覆盖Windows/Linux/RKMPP，不新增平台分支、线程、队列、公共参数或重试策略。输入快照在输出wrapper之前建立，不捕获新wrapper，不形成该包装器的反向强引用。

对象新增指针由现有MediaLineageAllocator的实际sizeof计费；av_buffer_ref产生的FFmpeg引用元数据沿用已有分配方式，本项不宣称完整外部分配预算闭合。硬件context及pool的实际存活期延长至最后snapshot像素引用释放，这是必要所有权，需在后续完整存储评估中覆盖。

验收仍使用原规格Release realtime三路120秒与所选音视频重入，检查清理失败语义和payload归零。串行准入超期与r267长同步等待仍独立未解决；先Windows，再RKMPP。无异常栈时只能证明本缺口已修，不能单凭一次不崩溃断言历史根因完全关闭。

双独立局部静态审查均PASS（r253_consumption_review_a、r253_consumption_review_b2）：引用对象、逆序析构、部分失败清理、无新wrapper反向环及共享平台路径成立。Release全量重建95859运行中；尚无此修复实流证据。

本机已有D:/Procdump/procdump.exe v11.1且EULA已接受，以及Windows Kits x64 cdb。下一运行对精确CLI PID附加procdump -ma -e -n 1，dump写项目out/acceptance；不安装、不改注册表、不使用启动wrapper。调试器附加可能影响时序，运行归为崩溃诊断，不能替代最终不附加调试器的完整验收。

Release全量重建95859成功。r271实流在首轮源期间仍因serial超期失败，CLI错误退出1、payload归零，无访问违规及dump；此为清理修复的有限实流支持，不关闭历史崩溃根因或重入验收。详见realtime-composition-r271-validation.md。
