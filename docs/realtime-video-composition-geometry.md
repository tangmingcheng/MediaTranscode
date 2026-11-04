# 固定网格几何规划

## 合同与依据

`MediaVideoCanvasGeometryPlanner` 接收按输入顺序排列的权威视频尺寸、SAR 和输出硬件表面的 software pixel format。返回独立 cell 与内容矩形；只有内容矩形交给既有 canvas tile copy。两路左右、三/四路 2×2；三路第四格由黑画布保留。输出 SAR 明确为 1:1，由返回产品传入唯一编码准备，不能继承选定音源的视频 SAR。

- [GStreamer compositor keep-aspect-ratio](https://gstreamer.freedesktop.org/documentation/compositor/)：内容保持显示比例、在目标格居中，未覆盖区域使用背景。
- [FFmpeg 6.0 scale_eval](https://ffmpeg.org/doxygen/6.0/scale__eval_8c_source.html)：复用 decrease 模式的整数比例取整、限制到目标矩形和 divisible-by 向下对齐策略。这里以 `width×SAR.num : height×SAR.den` 表示显示比例，并按输出格式分别取得水平/垂直色度采样对齐；不使用平台专属几何算法。

奇数色度单元按网格边界分配到后面的格，内容居中偏移也按色度单元对齐。整数像素及色度对齐会造成有限的比例舍入与一单元内的居中偏差；这与任意拉伸到整个 cell 不同。无法形成非零内容、缺少 SAR、输出表面格式无效或画布尺寸不符合色度网格时，在构图前失败。

## 执行边界

纯 planner 计算，无工作线程、运行队列和媒体帧所有权；最多四个 cell/content 产品。实际解码尺寸和 SAR 必须与权威源事实校验一致；实际 filter 输出、硬件 copy 能力和帧池仍需准备事务验证，不能由几何产品冒充能力证明。源 filter 以内容矩形尺寸为目标，画布按固定 square-pixel 语义编码。

本切片仅完成代码和静态自审；未执行构建或真实媒体验收。Windows→RKMPP 原规格 120 秒完整合屏验收门禁保持未通过。
