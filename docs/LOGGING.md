# Logging Guide

- 工程内置了基于 spdlog 的现代日志：异步、彩色控制台、滚动文件、源码位置信息、定期刷盘。
- 日志实现位于 `src/Log.cpp`，接口在 `include/Log.hpp`，宏：`LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL`。


在 VS Code 中可使用“运行和调试”选择 `(gdb) log_demo`，输出在“集成终端”中显示彩色。

## 日志文件

- 路径：`Log/server.log`，自动创建 `Log/` 目录
- 轮转：单文件 5MB，最多 3 份（`rotating_file_sink`）

## 运行时调节日志级别（环境变量）

- 通过 `SPDLOG_LEVEL` 控制级别：
  - 全局：`SPDLOG_LEVEL=info ./server`
  - 针对指定 logger：`SPDLOG_LEVEL="Server=debug,other=warn" ./server`
- 进程内可调用：`Server::setLevel(spdlog::level::info);`

## 编译期裁剪

- 默认 Debug 构建启用 `TRACE`，Release 构建启用 `INFO`：
  - 由 `include/Log.hpp` 中的 `SPDLOG_ACTIVE_LEVEL` 设定。
  - 可在编译命令中覆盖：`-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN`。

## 彩色输出常见排障

- 请在“集成终端”或外部终端查看输出；VS Code 的“调试控制台”不保证 ANSI 颜色。
- 确认未设置 `NO_COLOR` 环境变量：
  ```bash
  echo $NO_COLOR
  unset NO_COLOR
  ```
- 确认未定义 `SPDLOG_NO_COLOR` 编译宏。
- 颜色标记由 pattern 的 `%^` 和 `%$` 控制；本工程已为控制台 sink 设置。

## 刷新与可靠性

- `warn` 及以上级别即时刷盘；后台每 2s 定期刷盘。
- 退出时可调用 `Server::shutdownLogger()` 以确保全部刷盘。

## 源码位置信息

- 控制台与文件 pattern 含 `[%s:%# %!()]`，方便定位文件/行/函数。

## 参考

- spdlog 文档：https://github.com/gabime/spdlog
