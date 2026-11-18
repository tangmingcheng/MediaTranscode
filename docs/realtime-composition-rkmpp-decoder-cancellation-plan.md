# RKMPP 解码取消接线调查

状态：EOS输入等待候选已实现并编译；共享停止状态已提交，FFmpeg协作取消候选正在构建，项目callback owner移交尚未实施，未通过功能验收。依据rk516真实CLI失败栈；rk517未复现不能关闭。Windows FFmpeg不修改，目标依赖如需修复必须新前缀和旧版本回退。

## 2026-11-18 等待点收窄

目标MPP提交560734e83e7d3d8a0a2a337d07108f04f4030caa；实际mpp/mpp.c:433在尚未提交packet的EOS预留阶段强制MPP_POLL_BLOCK，普通输入获取则使用mInputTimeout。候选补丁tools/dependencies/mpp-eos-input-timeout.patch仅令预留遵循同一输入timeout，保留未提交错误返回和RET任务缓存；已应用并构建独立候选前缀，见下文；不代表取消完成。

零拷贝路径在成功enqueue后仍有BLOCK等待，不能改成超时/EAGAIN并重送已提交packet。H264/HEVC的FFmpeg普通mpp_packet_init及EOS走复制输入，不经过该零拷贝等待；当前合屏支持的H264/HEVC可继续推进协作取消，MJPEG init_with_buffer不能据此宣称支持。独立源码调查确认此范围区分；官方对照为[MPP mpp.c](https://raw.githubusercontent.com/rockchip-linux/mpp/develop/mpp/mpp.c)。仍需处理get_frame、info-change重试及close/reset的实际返回边界。

目标依赖已有未提交编码器修改：MPP的mpp/codec/mpp_enc_impl.c，FFmpeg的libavcodec/rkmppenc.c及rkmppenc.h；不得覆盖或丢弃。应用候选前须保存完整现状哈希/差异，以独立候选前缀构建，当前prefix-mpp/prefix-ffmpeg保持可回退。

进一步接线证据：MediaGraphWorker现有停止状态是私有原子，外部codec无法读取；现有共享ExitToken有已规划的实际分配几何，可审查将停止发布合入稳定生命周期token，避免新增未计费注册表。VideoDecodeNode绑定codec之后、首次send/receive之前可由owner绑定该worker的取消状态；准备阶段则使用已有MediaPreparationControl的deadline/stop_token。禁止从interrupt并发读取或改写codecContext。具体类型/ABI和移交设计仍须冻结后双审，不能只做MPP一行修改即判rk516修复。

## 官方合同与实际缺口

[FFmpeg收发合同](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)禁止send与receive同时EAGAIN。[ffmpeg-rockchip后端](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/master/libavcodec/rkmppdec.c)仍有pending packet等待后重试循环，因此不能把100ms等待到期直接返回EAGAIN作为修复。目标92aec19同类循环内的有限等待不等于API调用可取消；EOS/draining还有无限等待。不能声称找到可直接应用的官方修复。

[FFmpeg AVIO取消合同](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavformat/avio.h)通过回调返回取消，阻塞函数返回AVERROR_EXIT；可复用协作取消策略，但它属于libavformat，AVCodecContext没有自动获得该接口。项目FFmpegObservedReadAvio已有原子release/acquire取消模式。

## 拟定接线及冻结前必查项

1. 目标RKMPP后端提供版本化私有注册/查询接口；独立callback/user-data，不覆盖已经用于get_format的AVCodecContext.opaque。准备阶段验证能力，首次解码前固定注册，运行中不修改callback；缺接口不得下游fallback。
2. token随codec RAII owner保活，覆盖prepared probe、prepared decoder移交、运行与avcodec_free_context。现有CodecContextDeleter的callbackOwner可承载组合owner；不能只在VideoDecodeNode第一次收context后才创建。
3. 保留ThreadedExecutor与RuntimeBranch先全部worker requestStop再interrupt的顺序。执行域启动前登记稳定token；interrupt只原子置位，不读取并发转移中的codecContext，不并发reset/free。generation flush不能撤销会话取消。
4. 本轮H264/HEVC的pending packet、EOS提交和draining均须检查取消；MJPEG是后续扩展能力门禁，不能据本轮补丁宣称可取消。有限等待只提供检查机会，正常收发进度算法不变。尚需核实各MPP调用是否真正有有限返回保证；不能承诺能中断不可返回的驱动调用。
5. owner线程返回AVERROR_EXIT。仅当对应token已取消，adapter才映射项目Cancelled并保留native code；否则仍为错误。保留startup timeout为原始主错误，worker返回join后释放codec。取消不是EOF。
6. 不新增媒体队列、工作线程或公开部署参数；token与注册owner成本需计入既有准备存储规划。能力版本、注册状态和取消退出原因应可观测。平台差异仅在codec adapter，公共DAG不分叉。

冻结前仍缺：私有接口具体ABI/导出方式、prepared取消到执行域取消的生命周期绑定、有限等待参数的权威来源、MJPEG/MPP所有等待点审计及预算计费。独立审查确认方向可行，不等于完整实现设计PASS。

验收须覆盖真实CLI启动失败后的退出、正常caller-stop、EOS/失源重入和原三源规格；保留原失败错误，不能用强制清理退出作通过。共享改动须Windows同规格回归。本计划不改变完整验证后再打包的顺序。

## EOS候选实施结果

两名独立审查者核对目标完整函数后限定SOURCE PASS。目标rkmppdec.c无设置MPP_PACKET_FLAG_EXTRA_DATA/extra_data的路径，mpp.c reset仅捕获该标记，因此旧mExtraPacket递归提交忽略返回风险不属于本轮普通H264/HEVC复制输入证据；不得宣称所有MPP模式安全。

8核增量构建及候选安装exit0；候选目录为/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp-cancel-eos-20261118，librockchip_mpp.so.0 SHA256=3cbb91f664f133e9e625f0c8bc4da8009e8f804b335bfed5c7fcdc3aaa8a8e4e。现有prefix-mpp同库仍为2f6f34046ab9601e7c9938d6e2f3e306424a9b02ab5d1ee765f1d4d2779587c8，与修改前相同，运行环境尚未切换。

首次仅覆盖install前缀导致.pc仍指旧目录，已重新配置CMAKE_INSTALL_PREFIX并增量构建安装。随后仅覆盖PKG_CONFIG_LIBDIR的检查被既有PKG_CONFIG_PATH优先级干扰而返回/usr/local；同时限定两变量后已确认解析候选前缀。下一步FFmpeg候选必须保持该路径隔离，不能只看目录存在。

回退材料保存在同base的rollback-cancel-eos-20261118：MPP/FFmpeg原有未提交diff、mpp.c.before、原CMakeCache、SHA256.before、实际补丁；保留供回退，不能当临时测试垃圾删除。原mpp.c SHA256=13f9b97511a89531a0f72682813b73a36e0bceb9584e1a42566ccd823be94008；三个编码器源码哈希前后保持一致。配置回退应恢复原安装前缀；源码回退仅反向应用本补丁或恢复已核验原mpp.c，不能重置整棵依赖树。

本轮未运行媒体验收、未执行产品打包。只读审查用本机out/dependency-review/mpp.c副本已在记录后删除；目标候选与回退材料有意保留。下一步仍为FFmpeg协作取消接口及公共worker停止状态接线，不以本候选关闭rk516。

## 共享停止状态实施（未接 codec）

停止原子已由 MediaGraphWorker 私有成员迁入同一 compiled execution 的共享 MediaGraphWorkerExitToken；只有一份状态，release 发布、acquire 读取，取消不能撤销。沿用现有 token 分配及实际 make_shared 几何计费，不增加线程、媒体队列或公开参数。exited/completed 仍表示原完成条件，停止请求不等于退出。方案对照 [C++ 共享单调 stop-state](https://eel.is/c++draft/thread.stoptoken.intro)，本类型只复用其所有权与轮询策略，不声称实现标准 stoppable_token concept。

两名未参与实现者独立 SOURCE PASS。生产主图重新启动要求重新编译并更换 context/token；动态 branch 单次启动，初始提取仅转移尚未启动的 token。底层 executor 的孤立 clear→start 不得复用已取消 token；构造新 worker 不清零共享停止。缺 token 仍在启动前失败，析构停止空指针安全。先遍历 worker.requestStop（其中发布停止并唤醒该 worker），再取消 payload waiters 并调用 node.interrupt 的顺序未改，原首错记录未改。

当前只提供稳定取消读取点，尚未把 prepared codec owner 绑定到它，也未新增目标 FFmpeg 注册接口，不能判 rk516 已修复。RKMPP 8 核增量构建 135 项 exit0，实时 CLI SHA256 为 112ad879ed9f10c069cd7c74e09e10f762d12c8a509bd8a617bb6298c8f042c5；保留已有 FFmpeg deprecated 声明告警，未运行媒体实流，后续完整接线结果另行记录；Windows 共享链路回归仍待补，不改 Windows FFmpeg，不运行打包。

## 目标 FFmpeg 协作取消候选

`tools/dependencies/ffmpeg-rkmpp-decoder-cancellation.patch`提供私有 ABI v1：`avcodec_rkmpp_interrupt_version`查询已打开 H264/HEVC 后端；`avcodec_rkmpp_set_interrupt_callback`在首次解码前一次注册独立 callback/user-data，拒绝重复或迟到注册，flush不撤销注册。该接口不是上游 FFmpeg 公共 API，也不是对全部 MPP 调用可中断的保证。

复用后端既有100ms pending-input等待节奏，在无限/长排空等待内轮询取消；正常进度算法不变，没有新增对外超时EAGAIN出口。pending packet、EOS提交前、排空与info-change重试检查取消，返回AVERROR_EXIT；已获得frame走原统一释放，已接受packet不重送。必须与已归档MPP EOS输入预留timeout补丁配套使用。依据仍是上述FFmpeg收发进度与AVIO协作取消合同。

初审发现未启用COPY_OPAQUE但注册callback的draining遇discard/errinfo时仍可能EAGAIN转EOF，已补齐消耗帧释放后内部重试；修正后两名独立审查者均给限定SOURCE PASS。目标原解码源码和既有编码器未提交修改、配置与哈希已保存在`out/dependency-work/rollback-decoder-cancel-20261118`，应用前`git apply --check`成功；候选配置/构建使用独立`prefix-ffmpeg-cancel-20261118`及`prefix-mpp-cancel-eos-20261118`，当前产品运行前缀不切换。

尚缺：构建/导出/实际加载验证，以及项目类型化callback owner、准备control到decoder worker token移交、取消原因映射和原规格实流；不能据源码PASS关闭rk516。reset/destroy、buffer同步及不返回的驱动调用仍不在本接口可中断承诺内。

项目侧接线已收窄：将现有CodecContextDeleter.callbackOwner改为类型化组合owner，随CodecContextPtr已有转移路径保活，无需在BuildResult/PreparedVideoDecoder/factory复制第二份owner。FFmpegCodecContextBuffer只提供有效owned context的typed accessor；borrowed没有owner，不能从opaque猜测。准备/重开传既有control，直接resolver先绑定其执行token，再由VideoDecode两个入口统一移交到decoder token。移交不得撤销已发生取消，运行后不能继续使用准备deadline；owner查询不得分配。存储规划由sizeof(AVPixelFormat)改为实际组合状态大小。该项目接线尚未编码，AVERROR_EXIT须保留native code并区分取消与准备deadline。

当前目标构建进程make PID2437283（父Python2429304），通过现有源码树make -j8增量执行，未clean；候选前缀/MPP路径变更触发较多对象重编译。尚未收到最终退出码，不能记录构建通过。原安装libavcodec.so.62 SHA256=cbe8b3562f95af8286a3756c5683e995df8e616531cfa8f90b9c0d4de7d340fd，原MPP库SHA256=2f6f34046ab9601e7c9938d6e2f3e306424a9b02ab5d1ee765f1d4d2779587c8；构建后须复核未被替换。
