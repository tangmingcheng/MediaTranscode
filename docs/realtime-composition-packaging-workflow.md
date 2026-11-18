# RKMPP 打包流程规范化

两个入口已实现：`package_release.py`将现有Release构建生成SDK发布目录，`package_portable.py`从发布目录的程序生成依赖自包含的运行目录；`_package_support.py`仅共享校验。旧shell入口已替换，命令和边界见根目录deployment.md。

2026-11-18完成两名未参与实现者的源码复审PASS。首审发现示例缺RGA显式链接、dlopen加载名丢失、完成标记过早撤销；已全部修正并复审。实际采用构建缓存的RGA/ar/ranlib，加载名与SONAME实体保留，全部目录检查后才撤销.INCOMPLETE，归档临时写完后无覆盖发布。

本机Python AST语法检查、两个入口--help及git diff --check通过。未运行两阶段打包，未生成新SDK/portable/归档，未执行ARM程序；这是用户明确要求的执行边界，不能以源码通过宣称交付验证完成。后续真实打包、解包和运行仍在合屏完整验收之后；本项不阻塞合屏主任务。

未完成的合屏启动/失败退出、Windows回归、内容级AV等门禁保持，见合屏实施计划。源码提交声明与产物匹配由交付者核实，脚本保存哈希不替代证明；dlopen加载名和来源须有平台实际证据。
