# RTP 配置存储费用

本产品替换新 source-presentation Session 的旧 `workingSet.configurationBytes` 估算；旧 cadence/Raw 工作区接口不改变。未构建、未运行，不能据此判完整 G 通过。

## 实际 owner 与峰

- 常驻：Factory 创建的 depacketizer 持有一份 config；ProtocolSession 独立持有一份 config 和 envelope。两个 config 各含 codecName/fmtp，envelope 含 codecName/sizeAuthority/completionAuthority。Session 的 m_plan 已由 F 单独计费，不重复。
- Factory::validate 与 ProtocolSession::create 内第二次 validate 顺序互斥，取同一验证峰，不相加；第二次验证时第一份 depacketizer config 已常驻。
- parseRtpFmtp：按原文本分号、等号及原 trim 规则只读计算长度，保留 map 节点/key/value 和当前 token、trim 输入/输出、lowercase 移动留下的 proxy。不会另写协议 parser 或通过候选试分配测峰。
- map 返回经过 success 参数、Result 构造参数、optional，三个 moved-from map 各留下 sentinel/proxy；不是三份 map 数据。Base64/hex vector 返回对应三个空 proxy；数据 backing 移动不复制。
- H.264/HEVC 参数集逐个验证，decoded vector、substring、at() 临时 key、owner 字符串按单个最大片段计峰；hex、整数查询、模式比较与这些阶段互斥。
- ProtocolSession 的五个字符串在 create 参数、私有构造参数、success 参数、Result 参数、optional 间移动。最终 owner 已计费，其余四组只增加 Debug proxy；不会把这些 move 当作四份长字符串拷贝。

## 平台依据

直接核验本机 `D:/VisualStudio2026/VC/Tools/MSVC/14.51.36231/include/xstring` 的 `_Construct`、`_Construct_from_iter`、`_Calculate_growth`、`_Allocate_for_capacity`、SSO/terminator，以及 `xtree::_Alloc_sentinel_and_proxy`。fresh string copy 费用为真实 SSO 分支或 roundup/geometric capacity 加终止符；保留原容器 capacity readback 接口。map/vector 复用已有 `MediaStandardContainerGeometry` 的相同 supplier capability，不给 Runtime 新增门禁。

标准依据：[string.capacity](https://eel.is/c++draft/string.capacity)、[allocator requirements](https://eel.is/c++draft/allocator.requirements.general)。供应商具体增长不是 C++ 标准保证，因此此纯几何接口明确沿用已审计 C++20 MSVC 能力边界，不对未知 STL 猜测倍数。

## 消费

`MediaRtpConfigurationStoragePlanner::plan(config,envelope)` 返回常驻与峰值；`MediaPreparedRtpSourceStoragePlanner` 的新 source 分支使用 `ordinary + configuration.peakBytes` 形成普通分配保留。该保留覆盖未迁移 std allocator 的真实请求，不附加 PMR header，也不把费用当作 RSS。
