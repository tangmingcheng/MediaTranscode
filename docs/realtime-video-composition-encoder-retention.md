# 音频编码提交驻留契约

## 工业依据与部署身份

[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)要求发送受阻后接收推进，同一端重试不能依靠等待推进。共享节点保留原有单 owner、每次发送后接收至 EAGAIN 的执行方式；输出背压期间不提交新帧。

本机 `D:/mabs/build/ffmpeg-git` HEAD 为 `20054712a242c54aa19d9ae9fca7a0381a2f1397`，受版本控制文件无修改。`D:/mabs/local64/bin-video/ffmpeg.exe -version`实际返回 `N-125148-g20054712a2-g9cbd889670+3`、libavcodec `62.36.101`。release 目录、`3rds/ffmpeg/bin`与该部署目录的 `avcodec-62.dll` SHA256 均为 `F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621`。

这些证据仅覆盖该版本的 native `aac`。后续若实现适配器，需要同时核对 backend 名、库版本、build identity、frame size、initial padding 和样本时间基。RKMPP 尚未取得对应源码与部署证据，不能据此宣称支持。

## 上界来源

本地对应源码 `libavcodec/aacenc.c:812–822`先加入输入、首调用不出包；`:1095`每次编码从音频帧队列移除一帧；`:1164–1165`设置帧长与 priming。`audio_frame_queue.c:29–33,55–63`将初始 padding 计入首个负 PTS 包。`encode.c:544–575`允许 send 内部先生成并暂存一个包。

设 prepared 帧长为 F、initial padding 为 P。在每次发送后接收至 EAGAIN 的调用方式下，首调用保留 F、priming 包占用 P 个输出样本，提交后尚未接收的当前帧增加 F。因此该版本 native AAC 最大已提交未映射样本可由 F + P + F 推导；每个非空片段至少一个样本，片段上界可取同一样本数。这是源码推导，不是运行结果；不能将三帧常量写进共享 mapper。

未来 prepared probe 应使用相同已打开配置，逐项核对首调用无包、priming、首次真实映射、稳态一入一出及 drain。probe 只能核对已证明状态机，不能把有限观测的最大值当作硬上界；应使用独立 capability context，不消费生产 encoder。

## 本轮实际修改

本轮只保留 mapper 的发送前事务。发送前复制并验证候选区间；成功发送后只执行 noexcept swap。EAGAIN 时销毁候选、保留原 pending frame/fragments，接收后重新准备。提交后立即释放旧快照，避免接收阶段额外持有。事务为内部 move-only、单次提交对象，只在现有 lineage lock 内局部存活；其存活期间禁止其他 mapper 修改。

既有 submit 已复制整份 accumulator，再在发送后验证并提交；本轮将相同验证和分配提前至发送前。EAGAIN 重试会丢弃并重建候选，存在额外元数据分配开销。事务暂态仍有原区间和候选区间两份元数据；没有新增 PCM 拷贝。原 packet map、priming 和 reset 语义保持不变。

## 尚未解决的边界与范围

既有 provider 仍读取音频 encoding 未定义的 `context->delay`，经 `encoderDelaySamples`形成 `encoderLookaheadSamples`。当前消费者只将其加进修正命令在途样本上界，没有独立算法 lookahead 含义。后续修复应改为语义明确的在途产品，但本轮没有将 priming 或观测峰值冒充该事实。

将精确版本白名单立即作为全部同步音频转码的准入门禁，会拒绝既有 libfdk_aac、MP3、Opus、Vorbis 等 encoder，以及不同版本 native AAC。这属于支持范围收缩，未经确认不能作为常规修复交付。也不能通过未知 backend 不设界、继续使用 delay 或回退经验常量伪称边界已完成。因此本轮未保留 typed retention 产品、probe、门禁或 servo 改名代码。

下一步需为实际支持的 encoder 与部署版本取得权威状态机证据，或明确批准收窄支持范围；然后将 typed `maximumSubmittedUnmappedSamples`、片段上界与独立 priming 事实贯通 planner、builder、factory、mapper，并重新计算 command lead/FIFO 等关联产品。完整物理元数据预算亦尚未建立。

## 证据边界

驻留调查阶段仅执行了 diff 空白检查；随后主任务已完成 Release 构建和 r21 真实链路，结果见[事务验证记录](realtime-video-composition-transaction.md)，其中恢复仍 FAIL。44100 Hz、F=P=1024 时，拟采用在途项为3072样本，约69.66 ms；最终 command lead 还取决于其他队列和 measurement lead，须在后续真实规划与验收中核对，不能缩小上界绕过门禁。不得将驻留源码推导记为已实现或运行通过。
