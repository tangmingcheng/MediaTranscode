# 已封闭输入代的时钟观察边界

rk520已证明有序排空被ClockEvidenceExpired抢占，清除72项/72704音频样本。改动仅修复已接纳、有限输入的排空权限；不修改活跃源超时、公共参数、外部依赖或发送期限。

行业合同沿[GStreamer EOS](https://gstreamer.freedesktop.org/documentation/additional/design/events.html#eos)的数据顺序完成、与FLUSH丢弃区分；[RFC3550](https://www.rfc-editor.org/rfc/rfc3550)成员退出不提供取消本地已接纳媒体的要求。既有SourceDrain负责截止、frontier和逐成员完成；本次补齐与时钟有效域的连接。

内部类型携带现有playback generation、成员ingress generation/SSRC、真实closeDeadlineNs。时钟新证据只可能来自截止前捕获的数据，因此该代有效性只评价到此截止；SR/CNAME原始观测时间不变。截止本身已越过有效期仍按原合同失效，不能复活已clear的validator。普通活跃源始终使用真实now。

source owner在原mutex内统一应用边界到evidence、RTCP评价和clock transition；reorder expire、接收活动和诊断仍用真实时间。group节点只读取值类型边界，validator校验代/成员后按各自边界评价；显式invalidations优先，retirement后不再持有旧边界。mapper在group里只按实际SR观测时刻取得calibration，packet binder按已发布校准映射，不新增mapper fallback。

线程/内存：沿用source mutex、节点单owner和既有generation arbitration，不新增线程、锁、队列或持久分配；边界是栈上值，不扩大存储预算。背压继续由原packet/control额度控制。过期时间超出关闭范围不再作为credit等待计时器，避免空转；保留重排/截止/credit通知。

验证：两名独立源码复审；目标RKMPP原目录8核增量构建；原规格四源联合抓包复验首代及重入音频尾部。共享修改后Windows同规格回归仍必需。保持rk520发送期限FAIL与内容周期峰歧义，不以一次未复现关闭。

## 当前进度

2026-11-19：两名未参与实现的独立审查者均给出 SOURCE PASS；截止等值计时器问题已修正并获复审通过。RKMPP 原构建目录 8 核增量构建完成 44/44，退出码 0。首次同步因 CRLF/LF 哈希差异在写入前中止，随后旧源码的 no work to do 不计入验证；规范化前置校验通过后同步 12 个源码文件，逐文件校验成功，再完成上述实际构建。外部依赖保持原候选前缀。

下一步直接进行 rk521 原规格四源与指定音源实流复验，核对音频尾部、全失源及重入；Windows 共享链路回归仍待执行。构建和源码审查不代表实流修复通过，rk520 的内容尾部与发送期限失败仍保留。打包执行继续置于功能完整验证之后，不阻塞合屏主线。
