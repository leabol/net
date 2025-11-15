# 构建与工具链

## CMake 结构
- `net_core` 静态库聚合核心源码；demo 与 `core_build_test` 仅链接该库，缩小重编范围。
- 导出 `compile_commands.json`：便于 clangd/clang-tidy/IDE。

## clang-format/clang-tidy
- `format` 目标：全项目格式化（根据 `.clang-format`）。
- `tidy` 目标：收窄到 `net_core` 源，排除 `src/old` 与 demo/test 降噪。
- `.clang-tidy`：启用 `bugprone/*, modernize/*, performance/*, readability/*`，禁用“后置返回类型”、“短标识符长度”。

## 日志与调试
- spdlog：异步日志，控制台 + 滚动文件；`SPDLOG_LEVEL` 环境变量可动态调级。
- VS Code/调试：建议使用集成终端保留 ANSI 颜色；断点调试时关注非阻塞路径的 `errno` 读取时机。

## 平台/链接
- Linux 版本足够新以支持 `accept4`；若需更强移植性，可在 CMake 做功能探测并提供回退。
- `std::thread`/spdlog 异步在某些平台需要 `-pthread`；若遇链接问题请在目标上添加该选项。
