## `src/internal/graph/`

`src/internal/graph` 是项目中的 DAG 化媒体处理管线目录，负责描述、构建、校验、编译和运行媒体处理图。

```text
src/internal/graph/
  core/
    graph 基础模型目录。

  model/
    graph 共享数据模型目录。

  planner/
    媒体处理策略规划目录。

  builder/
    graph 构建目录。

  runtime/
    graph 运行时目录。

  nodes/
    具体运行时节点目录。

  diagnostics/
    graph/runtime 诊断辅助目录。

  preset/
    预定义 graph/pipeline 入口目录。
```

## `core/`

`core/` 是 graph 的基础模型目录。

主要作用：

```text
- 定义和维护 MediaGraph
- 定义 node、port、edge 等基础结构
- 管理节点、端口和边的连接关系
- 提供 graph 拓扑排序能力
- 提供 graph 结构校验能力
- 保存 graph 层面的错误定义
```

该目录关注“图本身是否成立”。

## `model/`

`model/` 是 graph 子系统共享的数据模型目录。

主要作用：

```text
- 定义 MediaNodeKind 等节点类型
- 定义 stream kind、payload kind、edge kind
- 定义 format、time、hardware 等描述符
- 定义 queue policy 和 threading policy
- 定义转码参数模型
- 定义 graph 中通用的基础类型
```

该目录关注“图中流动的数据、边和节点应该如何被描述”。

## `planner/`

`planner/` 是媒体处理策略规划目录。

主要作用：

```text
- 探测 FFmpeg codec 能力
- 探测硬件加速能力
- 选择音视频处理策略
- 评估候选处理路径
- 生成 builder 可消费的 plan 数据
```

该目录关注“应该选择哪条处理策略”。

## `builder/`

`builder/` 是 graph 构建目录。

主要作用：

```text
- 根据参数和 plan 创建 MediaGraph
- 创建 node、port、edge
- 连接 graph 拓扑
- 组合可复用 segment
- 构建本地文件 graph
- 构建 realtime graph
- 写入节点 option
```

该目录关注“如何把策略和参数转换成 DAG”。

### `builder/local/`

`builder/local/` 是本地文件处理 graph 的构建目录。

主要作用：

```text
- 构建本地文件转码 graph
- 构建本地文件输入到输出的处理链路
- 将本地文件构建参数转换为 planner request
```

### `builder/realtime/`

`builder/realtime/` 是实时媒体处理 graph 的构建目录。

主要作用：

```text
- 构建 realtime packet relay graph
- 构建 realtime ingest-to-mux graph
- 管理 realtime graph 构建参数
- 创建 realtime edge policy
- 写入 realtime 输入、输出和 SDP 相关节点 option
```

### `builder/segments/`

`builder/segments/` 是可复用 graph 片段目录。

主要作用：

```text
- 构建输入 segment
- 构建输出/mux segment
- 构建 video branch
- 构建 audio branch
- 构建 transcode branch
- 构建 packet-copy branch
- 提供 branch endpoint 校验
- 提供 branch option 映射
- 提供音视频 option applier
```

该目录用于减少不同 graph builder 之间的重复拓扑构建逻辑。

## `runtime/`

`runtime/` 是 graph 运行时目录。

主要作用：

```text
- 编译 MediaGraph
- 创建 runtime execution context
- 创建和管理 channel
- 根据 edge policy 创建 queue
- 注册和创建 runtime node
- 调度 runtime node 生命周期
- 管理 threaded graph execution
- 管理运行、停止、flush、abort、reset 等生命周期
```

该目录关注“已经构建好的 graph 如何运行”。

### `runtime/context/`

`runtime/context/` 是运行时上下文目录。

主要作用：

```text
- 保存 graph 编译后的执行上下文
- 校验 graph
- 建立 edge 到 channel 的运行时映射
- 保存执行顺序
```

### `runtime/channel/`

`runtime/channel/` 是运行时 channel 目录。

主要作用：

```text
- 表示 graph edge 在运行时的数据通道
- 管理 channel 的创建和查找
- 连接 runtime node 之间的数据流
```

### `runtime/queue/`

`runtime/queue/` 是运行时队列目录。

主要作用：

```text
- 定义 queue 抽象接口
- 根据 queue policy 创建具体 queue
- 提供 blocking queue
- 提供 SPSC queue
- 提供 SPSC ring queue
```

### `runtime/scheduler/`

`runtime/scheduler/` 是运行时调度目录。

主要作用：

```text
- 按 graph 执行顺序调度 runtime node
- 管理 node 的 configure、start、process、flush、stop 等生命周期调用
```

### `runtime/threading/`

`runtime/threading/` 是多线程执行目录。

主要作用：

```text
- 管理 threaded graph executor
- 管理 runtime worker
- 支持每个 node 或每类 node 的线程化执行
```

### `runtime/factory/`

`runtime/factory/` 是 runtime node 工厂目录。

主要作用：

```text
- 根据 MediaNodeKind 创建具体 runtime node
- 注册默认 runtime node 类型
- 将 graph node 类型映射到 nodes/ 中的实现类
```

### `runtime/buffer/`

`runtime/buffer/` 是运行时 buffer 目录。

主要作用：

```text
- 定义运行时 buffer 引用
- 封装 FFmpeg packet/frame 等媒体数据
- 支持 runtime channel 中的数据传递
```

### `runtime/ffmpeg/`

`runtime/ffmpeg/` 是 FFmpeg 运行时辅助目录。

主要作用：

```text
- 提供 FFmpeg packet/frame/buffer 辅助类型
- 提供 FFmpeg 错误处理辅助能力
- 支持 nodes 和 runtime 对 FFmpeg 数据的统一访问
```

## `nodes/`

`nodes/` 是具体运行时节点实现目录。

主要作用：

```text
- 实现输入节点
- 实现 metadata/codec resolver 节点
- 实现 packet 处理节点
- 实现 demux/split/merge 节点
- 实现 video decode/filter/encode 节点
- 实现 audio decode/resample/encode 节点
- 实现 mux/output 节点
- 实现 debug/control/lifecycle/route 等辅助节点
```

该目录关注“单个节点如何处理自己的输入和输出”。

### `nodes/input/`

`nodes/input/` 是输入节点目录。

主要作用：

```text
- 实现文件输入节点
- 实现 realtime 输入节点类型
```

### `nodes/metadata/`

`nodes/metadata/` 是元数据和 codec 信息处理节点目录。

主要作用：

```text
- 解析 codec 信息
- 探测媒体 metadata
- 为后续 decode、encode、mux 节点提供必要上下文
```

### `nodes/packet/`

`nodes/packet/` 是 packet 处理节点目录。

主要作用：

```text
- 处理 packet 标准化
- 处理 packet source 配置
- 为 packet 流进入后续节点做准备
```

### `nodes/demux/`

`nodes/demux/` 是解复用和流拆分节点目录。

主要作用：

```text
- 从输入中 demux 出媒体流
- 将不同 stream 拆分到不同输出路径
```

### `nodes/split/`

`nodes/split/` 是分流节点目录。

主要作用：

```text
- 对 packet 或其他媒体数据进行 fanout
- 支持一个输入分发到多个输出路径
```

### `nodes/merge/`

`nodes/merge/` 是合流节点目录。

主要作用：

```text
- 合并多个输入路径的数据
- 为后续 mux 或 output 节点提供统一输入
```

### `nodes/video/`

`nodes/video/` 是视频处理节点目录。

主要作用：

```text
- 视频解码
- 硬件/软件帧转换
- 视频时间戳处理
- 视频帧率处理
- 视频滤镜处理
- 视频编码
```

### `nodes/audio/`

`nodes/audio/` 是音频处理节点目录。

主要作用：

```text
- 音频 codec 信息解析
- 音频解码
- 音频重采样
- 音频编码
```

### `nodes/mux/`

`nodes/mux/` 是复用节点目录。

主要作用：

```text
- 文件 mux
- RTP mux 节点类型
- 将编码后的音视频 packet 组织为输出格式
```

### `nodes/output/`

`nodes/output/` 是输出节点目录。

主要作用：

```text
- 文件输出
- RTP 输出节点类型
- SDP 写入节点类型
```

### `nodes/debug/`

`nodes/debug/` 是调试节点目录。

主要作用：

```text
- 提供 graph 调试辅助节点
- 观察或输出运行时数据
- 辅助验证 graph 数据流
```

### `nodes/control/`

`nodes/control/` 是控制节点目录。

主要作用：

```text
- 处理控制类 runtime node
- 支持 graph 运行过程中的控制信号
```

### `nodes/lifecycle/`

`nodes/lifecycle/` 是生命周期辅助节点目录。

主要作用：

```text
- 处理生命周期相关 runtime node
- 支持启动、停止、flush、结束等流程中的辅助行为
```

### `nodes/route/`

`nodes/route/` 是路由节点目录。

主要作用：

```text
- 处理运行时数据路由
- 支持 graph 内部不同路径之间的数据分发
```

## `diagnostics/`

`diagnostics/` 是 graph/runtime 诊断辅助目录。

主要作用：

```text
- 提供诊断名称
- 提供诊断消息
- 提供采样辅助
- 提供 trace 辅助
- 辅助 graph 和 runtime 问题定位
```

## `preset/`

`preset/` 是预定义 graph 或 pipeline 入口目录。

主要作用：

```text
- 保存预定义 graph/pipeline 配置
- 提供常用 pipeline 的入口封装
- 简化外部调用方创建标准 graph 的过程
```