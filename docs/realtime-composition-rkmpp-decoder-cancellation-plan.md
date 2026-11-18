# RKMPP 解码取消接线调查

状态：设计调查，尚未实现或通过功能验收。依据rk516真实CLI失败栈；rk517未复现不能关闭。Windows FFmpeg不修改，目标依赖如需修复必须新前缀和旧版本回退。

## 官方合同与实际缺口

[FFmpeg收发合同](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)禁止send与receive同时EAGAIN。[ffmpeg-rockchip后端](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/master/libavcodec/rkmppdec.c)仍有pending packet等待后重试循环，因此不能把100ms等待到期直接返回EAGAIN作为修复。目标92aec19同类循环内的有限等待不等于API调用可取消；EOS/draining还有无限等待。不能声称找到可直接应用的官方修复。

[FFmpeg AVIO取消合同](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavformat/avio.h)通过回调返回取消，阻塞函数返回AVERROR_EXIT；可复用协作取消策略，但它属于libavformat，AVCodecContext没有自动获得该接口。项目FFmpegObservedReadAvio已有原子release/acquire取消模式。

## 拟定接线及冻结前必查项

1. 目标RKMPP后端提供版本化私有注册/查询接口；独立callback/user-data，不覆盖已经用于get_format的AVCodecContext.opaque。准备阶段验证能力，首次解码前固定注册，运行中不修改callback；缺接口不得下游fallback。
2. token随codec RAII owner保活，覆盖prepared probe、prepared decoder移交、运行与avcodec_free_context。现有CodecContextDeleter的callbackOwner可承载组合owner；不能只在VideoDecodeNode第一次收context后才创建。
3. 保留ThreadedExecutor与RuntimeBranch先全部worker requestStop再interrupt的顺序。执行域启动前登记稳定token；interrupt只原子置位，不读取并发转移中的codecContext，不并发reset/free。generation flush不能撤销会话取消。
4. pending packet、EOS提交、draining和MJPEG等待均须检查取消；有限等待只提供检查机会，正常收发进度算法不变。尚需核实各MPP调用是否真正有有限返回保证；不能承诺能中断不可返回的驱动调用。
5. owner线程返回AVERROR_EXIT。仅当对应token已取消，adapter才映射项目Cancelled并保留native code；否则仍为错误。保留startup timeout为原始主错误，worker返回join后释放codec。取消不是EOF。
6. 不新增媒体队列、工作线程或公开部署参数；token与注册owner成本需计入既有准备存储规划。能力版本、注册状态和取消退出原因应可观测。平台差异仅在codec adapter，公共DAG不分叉。

冻结前仍缺：私有接口具体ABI/导出方式、prepared取消到执行域取消的生命周期绑定、有限等待参数的权威来源、MJPEG/MPP所有等待点审计及预算计费。独立审查确认方向可行，不等于完整实现设计PASS。

验收须覆盖真实CLI启动失败后的退出、正常caller-stop、EOS/失源重入和原三源规格；保留原失败错误，不能用强制清理退出作通过。共享改动须Windows同规格回归。本计划不改变完整验证后再打包的顺序。
