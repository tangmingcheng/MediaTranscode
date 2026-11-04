# RKMPP 合屏生产者契约实施计划

状态：正常 API 完成语义经两名独立审查者确认可采用；尚未完成实现或实流验收。当前失败仍为 rk386 的 CUDA-only 源准备门禁。

## 语义与依据

Completed 表示固定依赖的成功同步 API 已交付可消费帧，不需要引擎追加生产者等待；它不保证依赖所有异常均被传播，也不表示像素独立分配。MPP 输出通过 AVBuffer 持有 MppFrame，引用释放前不能回收借用表面。RGA 在正常输入路径的 imsync 成功后发布独立池输出；实际 async_depth 必须读回为 0，不能仅凭滤镜名称授权。

依据：[固定 MPP 输出实现](https://raw.githubusercontent.com/rockchip-linux/mpp/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/mpp.c)、[固定 vproc 实现](https://raw.githubusercontent.com/rockchip-linux/mpp/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/vproc/mpp_dec_vproc.c)；FFmpeg 部署 ab1e61a 的 rkmppdec.c、rkrga_common.c 已现场只读核对。依赖错误传播风险见 r253，不扩大外部修复范围，不强制关闭去交错。

## 实施顺序

1. 将共享 readiness 产品改为有类型的 Completed/CUDA producer variant；平台 adapter 提供明确身份与引用上界，未知类型必须失败。同步修改 options codec、runtime binding、失败保护及资源一致性检查。
2. 将 decoder 分配来源区分为独立输出与借用表面，保留实际 MPP AVBuffer 生命周期；不把 borrowed surface 填入现有独立 CUDA 分配承诺。
3. 复用现有 source isolation adapter 和 VideoFilterNode，为合屏 tile 提供 RGA 独立输出证据；即使尺寸相同也不能以 clone 替代隔离。filterExecution 的完成/分配证据与时序来源分开表达，准备、运行时及驻留计费消费同一产品。
4. SourcePreparer 使用同一 decoder→filter 流程，核对配置、输出池和帧域；同步返回错误原样保留。Completed 不创建 CUDA event，但不能省略引用、wrapper、credit 和 RGA 输入缓存驻留预算。
5. 双人冻结复审、Windows 共享链路验证、RKMPP 8 核增量构建与原规格 realtime 实流复验；正常基础链路打通后继续两端细致可靠性验证。构建成功不替代媒体验收。

## 边界

节点仍按现有线程模型运行，planner 负责能力与容量；不增加旁路媒体链。RGA 缓存保留量从已验证实现进入原 holder 计费，原队列及存储硬上限不放宽。错误/失源/重入维持原验收要求；API 已返回错误不能转换成 Completed。MPP/IEP 部分等待返回值未检查属于保留的依赖可靠性风险，本轮没有证据证明已发生未完成帧发布或释放安全缺陷。
