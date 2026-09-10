# 视频动态多输出实施记录

## 范围与状态

一路输入，动态增删独立视频输出；CLI 与 beta C API 复用生产 DAG。完整编码请求相等时复用编码组，协议输出保持独立队列与故障域；删除至零后继续消费输入。首版不实现音频多输出，首项验收 RTP → MPEG-TS/RTP，先 Windows 后 RKMPP。

- 已实现共享输入/解码、编码组、协议输出三类执行段，以及 prepared encoder、不可变元数据、producer 资源账户和引用退役。
- 已实现标准输入增删、逐输出状态、自然 IDR 加入、独立背压及有序 EOS。
- Windows 第13轮基础动态验收通过，独立提交并推送 `10165000`。
- Windows 第16轮编码组复用、异规格新组、删除共享消费者、零输出及恢复通过，独立提交并推送 `4bf43635`。
- 审查发现的固定10秒加入预算、控制序列同步驱动回收已修复；IDR周期来自encoder readback，加入沿用原事务期限，物理回收和失败候选清理由拥有资源的后台线程完成。
- RKMPP持有量、独立源帧分配与同步RGA证据已形成版本限定adapter，复用既有VideoFilter节点；Windows共享代码已全量构建，RKMPP新代码尚未验证。
- Windows第18轮出现VLC晚帧；第19轮将截图移到媒体结束后未再晚帧，但暴露FileMux缓存WouldBlock导致排空超时。已修复全部相关转发路径，独立源码审查PASS，正在原规格复验。
- Windows20/21排空与发送正常但VLC晚帧门禁失败，均已记录并清理原始材料；不把RC冻结统计作为零丢帧证据。
- 整批双审发现fail停止请求与后台回收竞态，已以同一publication锁完成停止请求屏障修复；两者复审源码PASS，完整交付仍未完成。
- 架构与增量评分已更新；冻结41bb17a1在Windows第22轮通过原规格完整动态验收：H.264 RTP 720p30约8Mbps → HEVC/H.264 MPEG-TS/RTP 1080p25 CBR6Mbps GOP50。长GOP与RKMPP实流仍待验证。
- 草稿PR #33已创建，新独立智能体对41bb17a1源码审查PASS；完整交付待长GOP、RKMPP及最终证据复核，当前评分不自行上调。
- Windows第23/24轮分别通过GOP500实际等待17.114秒加入、GOP750超原事务期限提前拒绝，独立提交5cfdf3f6/de1344da。
- RKMPP第2轮共享源滤镜暴露VideoOnly解码帧时间基缺失，未输出媒体；共享解码边界修复已获双审源码PASS，正在Windows回归。另确认目标FFmpeg私有last_pkt关闭所有权缺陷，按硬件解码器成熟释放方式修复独立依赖版本，尚未验收。
- 修复冻结98742fd6已推送。Windows25动态状态、RTP/TS/时钟和引用归零正常，但VLC实际晚帧12帧，判FAIL；WPR CPU/GPU观测因当前终端非管理员被OS拒绝0xc5585011，依用户要求目前不使用WPR、不触发UAC，继续日志和抓包定位。新依赖ab1e61a及新RK CLI全量构建均退出0、哈希及ldd已记录，未启动新RK实流。
- 98742fd6源码双审及新PR审查均PASS，完整交付均FAIL；独立评分A86/B85/新PR81，详见QUALITY_SCORE.md，未自行合并或提高评分。

- Windows26补充诊断：仅增加VLC RC统计唤醒，四路完整日志无晚帧、RTP到达序号连续、引用归零；可能存在观测调度扰动，不替代Windows正式门禁，详见[记录](dynamic-video-windows-26.md)。

- Windows27撤除周期RC后，恢复路实际晚帧22ms/lost1，判FAIL；动态增删时刻与26不同，不据此宣称RC因果。用户已明确授权一次管理员WPR CPU/GPU及UAC，Windows28限定诊断采集正在进行。

## 行业依据与设计边界

| 功能 | 权威依据 | 实施约束 |
|---|---|---|
| 动态管线 | [GStreamer](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html) | 发布前准备、暂停点准入、EOS排空、继承会话时钟。 |
| 背压 | [GStreamer tee](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html)、[FFmpeg tee/fifo](https://ffmpeg.org/ffmpeg-formats.html#tee) | 每分支独立有界队列，失败不拖住其他消费者。 |
| 异步释放 | [GStreamer call_async](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelement.html#gst_element_call_async) | 拥有资源的线程执行阻塞释放；实际结束前保持引用及计费，无 detached 线程。 |
| 随机访问 | [RFC6184](https://www.rfc-editor.org/rfc/rfc6184#section-8.5)、[RFC7798](https://www.rfc-editor.org/rfc/rfc7798) | 检查真实NAL IDR与初始化参数，等待由真实encoder事实及原事务期限规划。 |
| 出口容量 | [Linux TBF](https://www.man7.org/linux/man-pages/man8/tc-tbf.8.html) | 同一service scope共用仲裁器，RTP/RTCP按实际提交线速字节同账。 |
| 硬件帧池 | [FFmpeg AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html) | 不将initial_pool_size请求视为所有后端硬上限；引擎payload和驱动内存分别计费/观测。 |

线程、队列、固定存储和payload由planner产品确定。驱动无通用可取消能力，超时只能判失败，不能提前销毁仍在使用的资源；硬件物理释放时长不承诺上界。

## 真实验证

| 轮次 | 结果 | 证据 |
|---|---|---|
| Windows01—12 | FAIL/不完整 | 见各轮报告，失败不回写为通过。 |
| [Windows13](dynamic-video-windows-13.md) | 基础动态PASS | 固定120秒H.264 RTP 720p30约8Mbps → HEVC/H.264 MPEG-TS/RTP 1080p25 CBR6Mbps GOP50；零丢失、聚合50Mbps曲线超额1356B、引用归零。 |
| Windows14—15 | 不完整/FAIL | 控制窗口遗漏、晚帧与截图失败。 |
| [Windows16](dynamic-video-windows-16.md) | 编码组动态PASS | 同源同规格，复用/新组/删至零/恢复；四路画面、VLC D3D11VA、RTP零丢失、引用归零。 |
| [Windows17](dynamic-video-windows-17.md) | FAIL，补充诊断 | 只改GOP750；复用组加入10秒后失败，暴露等待合同缺陷。 |
| [Windows18](dynamic-video-windows-18.md) | FAIL | 动态生命周期正常，但VLC出现过晚显示告警。 |
| [Windows19](dynamic-video-windows-19.md) | FAIL | 四路画面和发送正常，前两路FileMux未结束，触发排空超时；不能以最终workerErrors为零掩盖输出级失败。 |
| [Windows20](dynamic-video-windows-20.md) | FAIL | 排空正常，VLC49/21/21ms晚帧。 |
| [Windows21](dynamic-video-windows-21.md) | FAIL | 排空正常，VLC90/51ms晚帧；RC统计冻结不可用于验收。 |
| [Windows22](dynamic-video-windows-22.md) | PASS | 原规格动态增删/复用/零输出恢复、D3D11VA画面、无晚帧丢弃、RTP/TS/时钟/共享整形及引用归零。 |
| [RKMPP01](dynamic-video-rkmpp-01.md) | FAIL | 同规格准备阶段缺压缩输入持有adapter，输出0包。 |
| [Windows23](dynamic-video-windows-23.md) | PASS | 原规格GOP500，复用组等待真实IDR17.114秒后运行，源持续120秒，引用归零。 |
| [Windows24](dynamic-video-windows-24.md) | PASS | 原规格GOP750，原30秒事务期限内无法容纳完整加入预算，新增输出在发布前拒绝，原输出完成120秒。 |
| [RKMPP02](dynamic-video-rkmpp-02.md) | FAIL | 共享源隔离滤镜缺时间基，输出0包；payload单对象残留及依赖关闭缺陷单独保留，接收VLC绑定设置也需纠正。 |
| [Windows25](dynamic-video-windows-25.md) | FAIL | 新共享时间基代码动态状态/网络/引用正常，VLC初始11帧、第三路1帧晚帧丢弃，待调度定位。 |

Windows NVDEC safe-output可证明独立CUDA帧。RKMPP固定解码池关系不同，复用既有VideoFilterNode在分发前执行独立分配复制；统一滤镜时序事实，不建立平台专用媒体链路。适用范围见[源隔离证据](dynamic-video-source-isolation.md)及[RKMPP adapter证据](dynamic-video-rkmpp-adapter-evidence.md)。

## 清理

本机原始日志、抓包、截图和SDP只放D盘；每轮提取命令、结果和PID后删除并检查进程。RKMPP同样清理远端原始材料与临时控制文件。源视频和编译结果保留；原有未跟踪依赖头文件、out及NUL不纳入提交、不清理。
