# 日志系统使用指南

## 概述

本项目采用**spdlog**库实现专业的分级日志系统，支持6个日志级别，可根据需求灵活控制日志输出。

## 快速开始

### 基础运行

```bash
# 默认级别运行（Debug编译=TRACE，Release编译=INFO）
./build/http_file_server

# 自定义日志级别
SPDLOG_LEVEL=info ./build/http_file_server
```

### 常见需求快速参考

| 需求 | 命令 |
|------|------|
| 不想看到调试日志 | `SPDLOG_LEVEL=info ./build/http_file_server` |
| 只看警告和错误 | `SPDLOG_LEVEL=warn ./build/http_file_server` |
| 不输出到终端 | `./build/http_file_server 2>/dev/null` |
| 后台运行无终端输出 | `nohup ./build/http_file_server >/dev/null 2>&1 &` |
| 后台运行+只要重要日志 | `nohup SPDLOG_LEVEL=info ./build/http_file_server >/dev/null 2>&1 &` |
| 查看文件日志 | `tail -f Log/server.log` |
| 开发调试详细日志 | `SPDLOG_LEVEL=debug ./build/http_file_server` |
| 极详细调试 | `SPDLOG_LEVEL=trace ./build/http_file_server` |

## 日志级别说明

| 级别 | 优先级 | 用途 | 示例场景 |
|------|--------|------|---------|
| **TRACE** | 最低 | 极详细的调试信息 | 数据包详情、缓冲区操作 |
| **DEBUG** | 低 | 程序执行流程 | 函数调用、路由匹配 |
| **INFO** | 中 | 重要业务事件 | 服务启动、连接建立、文件操作成功 |
| **WARN** | 高 | 异常但可恢复 | 文件不存在、请求格式错误 |
| **ERROR** | 很高 | 可恢复的错误 | 读写失败、连接错误 |
| **CRITICAL** | 最高 | 系统级严重错误 | 初始化失败、致命错误 |

## 使用方式

### 1. 按级别运行

```bash
# 仅显示INFO及以上（推荐生产环境）
SPDLOG_LEVEL=info ./build/http_file_server

# 显示DEBUG及以上（推荐开发环境）
SPDLOG_LEVEL=debug ./build/http_file_server

# 显示所有消息（包括TRACE）
SPDLOG_LEVEL=trace ./build/http_file_server

# 仅显示WARNING及以上（最少输出）
SPDLOG_LEVEL=warn ./build/http_file_server

# 仅显示错误
SPDLOG_LEVEL=err ./build/http_file_server
```

### 2. 按模块控制（高级）

```bash
# 设置Server模块为DEBUG，其他为INFO
SPDLOG_LEVEL="Server=debug" ./build/http_file_server

# 多个模块设置
SPDLOG_LEVEL="Server=debug,http=info" ./build/http_file_server
```

### 3. 指定日志目录

```bash
# 默认日志目录：./Log/
# 自定义日志目录：
SERVER_LOG_BASE=/var/log/myserver ./build/http_file_server
```

### 4. 禁用或减少日志输出

```bash
# 完全关闭调试日志（仅保留INFO及以上）
SPDLOG_LEVEL=info ./build/http_file_server

# 仅输出警告和错误（不输出正常业务日志）
SPDLOG_LEVEL=warn ./build/http_file_server

# 关闭终端输出，仅输出到文件（重定向stderr）
./build/http_file_server 2>/dev/null

# 关闭所有输出到终端（stdout和stderr都重定向）
./build/http_file_server >/dev/null 2>&1

# 后台运行，不输出到终端
nohup ./build/http_file_server >/dev/null 2>&1 &

# 后台运行，仅保留文件日志，不输出到终端
nohup SPDLOG_LEVEL=info ./build/http_file_server >/dev/null 2>&1 &

# 仅保留错误日志到终端，其他到文件
SPDLOG_LEVEL=error ./build/http_file_server 2>&1 | tee error.log
```

## 日志输出格式

```
[时间戳] [Logger名称] [日志级别] [tid 线程ID] [源文件:行号 函数名()] 日志内容
```

### 实际示例

```
[2025-12-12 22:50:06.171] [Server] [debug] [tid 36738] [TcpConnection.cpp:37 connectEstablished()] TcpConnection fd=6 established
[2025-12-12 22:50:06.172] [Server] [info] [tid 36738] [HttpServer.cpp:87 handleRequest()] fd=6 GET /
[2025-12-12 22:50:06.708] [Server] [debug] [tid 36738] [HttpServer.cpp:241 replyFileList()] fd=6 file list: 6 files found
```

## 日志文件管理

### 文件位置

- **日志目录**：`./Log/` （或自定义 `SERVER_LOG_BASE`）
- **日志文件**：`server.log`
- **自动备份**：`server.log.1`, `server.log.2`, `server.log.3`

### 自动轮转策略

- **单个文件大小**：5 MB
- **备份份数**：3个
- **刷新策略**：
  - WARN级别及以上：立即刷新到磁盘
  - 其他级别：每2秒自动刷新

### 手动管理

```bash
# 查看当前日志
tail -f Log/server.log

# 查看所有日志文件
ls -lh Log/

# 清理旧日志
rm Log/server.log.*

# 统计日志大小
du -sh Log/
```

## 典型场景

### 场景1：生产环境调试

```bash
# 启动时启用INFO级别，记录所有重要事件和错误
SPDLOG_LEVEL=info ./build/http_file_server &

# 实时查看日志
tail -f Log/server.log

# 当发生问题时，临时提升日志级别（需要重启）
SPDLOG_LEVEL=debug ./build/http_file_server &
```

### 场景2：性能分析

```bash
# 使用INFO级别减少日志开销
SPDLOG_LEVEL=info ./build/http_file_server

# 运行测试
# ...

# 分析日志中的性能指标
grep "uploaded\|downloaded\|deleted" Log/server.log | wc -l
```

### 场景3：开发调试

```bash
# 启用DEBUG级别查看执行流程
SPDLOG_LEVEL=debug ./build/http_file_server

# 也可以启用TRACE查看所有数据流
SPDLOG_LEVEL=trace ./build/http_file_server

# 搭配grep过滤感兴趣的日志
tail -f Log/server.log | grep "GET\|POST\|DELETE"
```

### 场景4：生产环境部署（不输出到终端）

```bash
# 后台运行，关闭终端输出，仅保留文件日志
nohup SPDLOG_LEVEL=info ./build/http_file_server >/dev/null 2>&1 &

# 使用systemd服务（推荐）
# 创建 /etc/systemd/system/http_file_server.service
[Unit]
Description=HTTP File Server
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/http_file_server
Environment="SPDLOG_LEVEL=info"
ExecStart=/opt/http_file_server/build/http_file_server
StandardOutput=null
StandardError=null
Restart=always

[Install]
WantedBy=multi-user.target

# 启动服务
sudo systemctl start http_file_server
sudo systemctl enable http_file_server

# 查看文件日志
tail -f /opt/http_file_server/Log/server.log
```

### 场景5：不需要调试信息

```bash
# 方式1：设置日志级别为INFO（推荐）
SPDLOG_LEVEL=info ./build/http_file_server

# 方式2：设置为WARN（仅警告和错误）
SPDLOG_LEVEL=warn ./build/http_file_server

# 方式3：编译Release版本（自动排除TRACE和DEBUG）
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
./build/http_file_server  # 默认只输出INFO及以上

# 说明：
# - INFO级别：显示重要业务事件，不显示调试细节
# - WARN级别：只显示警告和错误
# - Release编译：DEBUG和TRACE日志在编译期被删除，性能最优
```

## 日志查询技巧

### 查找特定请求

```bash
# 查找所有GET请求
grep "GET" Log/server.log

# 查找特定文件操作
grep "uploaded\|downloaded\|deleted" Log/server.log

# 查找特定连接的所有日志
grep "fd=6" Log/server.log
```

### 查找错误

```bash
# 查找所有ERROR及以上级别的日志
grep "\[error\]\|\[critical\]" Log/server.log

# 查找特定错误类型
grep "connection.*error" Log/server.log
```

### 统计分析

```bash
# 统计请求数
grep "GET\|POST\|DELETE" Log/server.log | wc -l

# 统计错误数
grep "\[error\]\|\[warn\]" Log/server.log | wc -l

# 统计各级别日志
echo "TRACE:"; grep "\[trace\]" Log/server.log | wc -l
echo "DEBUG:"; grep "\[debug\]" Log/server.log | wc -l
echo "INFO:"; grep "\[info\]" Log/server.log | wc -l
```

## 编程中的日志使用

### 代码中添加日志

```cpp
#include "Log.hpp"

// 不同级别的日志
LOG_TRACE("Detailed debug info: {}", value);
LOG_DEBUG("Function entry: {}", funcName);
LOG_INFO("Important event: {}", message);
LOG_WARN("Warning condition: {}", warning);
LOG_ERROR("Error occurred: {}", error);
LOG_CRITICAL("System failure: {}", critical);
```

### 日志格式化

```cpp
// 支持类printf风格的格式化
LOG_INFO("Connection fd={} from {} completed {} bytes transfer",
         fd, address, bytesCount);

// 支持多种类型
LOG_DEBUG("Float: {:.2f}, Hex: {:#x}, Bool: {}",
          floatVal, hexVal, boolVal);
```

## 性能考虑

### 编译期优化

- **Debug编译**：编译级别为TRACE，所有日志都编译进去
- **Release编译**：编译级别为INFO，TRACE和DEBUG被优化删除

### 运行时性能

- **异步日志**：不阻塞主线程
- **8KB日志队列**：缓冲日志内容
- **1个后台线程**：统一处理日志I/O

### 优化建议

```bash
# 生产环境：INFO级别获得最佳平衡
SPDLOG_LEVEL=info ./build/http_file_server

# 高并发场景：WARN级别降低日志开销
SPDLOG_LEVEL=warn ./build/http_file_server

# 调试场景：DEBUG级别追踪问题
SPDLOG_LEVEL=debug ./build/http_file_server
```

## 常见问题

### Q: 日志文件变得很大怎么办？

A: 项目已配置自动轮转，单个文件限制为5MB，自动保留3个备份。若要更清晰的日志历史，可以：

```bash
# 定期清理
rm Log/server.log.*

# 或者提升日志级别减少输出
SPDLOG_LEVEL=warn ./build/http_file_server
```

### Q: 能否同时输出到文件和网络？

A: 项目目前支持控制台和文件输出。若需要网络上报，可在 `Log.cpp` 中添加网络Sink。

### Q: 日志线程安全吗？

A: 是的。spdlog使用线程池异步处理日志，完全线程安全。

### Q: 能否动态改变日志级别而无需重启？

A: 目前需要重启。若需要动态调整，可在 `Log.hpp` 中使用 `setLevel()` 添加运行时调整接口。

### Q: 如何关闭终端输出？

A: 有多种方式：

```bash
# 方式1：重定向stderr到/dev/null（推荐后台运行）
./build/http_file_server 2>/dev/null

# 方式2：重定向所有输出
./build/http_file_server >/dev/null 2>&1

# 方式3：使用nohup后台运行
nohup ./build/http_file_server >/dev/null 2>&1 &

# 日志仍会写入 Log/server.log 文件
tail -f Log/server.log
```

### Q: 如何关闭所有调试日志？

A: 有三种方式：

```bash
# 方式1：运行时设置日志级别为INFO（推荐）
SPDLOG_LEVEL=info ./build/http_file_server
# 此时不会输出DEBUG和TRACE日志

# 方式2：设置更高级别（仅警告和错误）
SPDLOG_LEVEL=warn ./build/http_file_server

# 方式3：编译Release版本（性能最优）
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
./build/http_file_server
# Release版本会在编译期删除DEBUG和TRACE代码
```

### Q: 生产环境推荐配置？

A: 推荐以下配置：

```bash
# 使用INFO级别 + 后台运行 + 无终端输出
nohup SPDLOG_LEVEL=info ./build/http_file_server >/dev/null 2>&1 &

# 或者使用systemd管理（更推荐）
# 设置 StandardOutput=null 和 StandardError=null
# 仅通过 Log/server.log 查看日志
```

## 相关文件

- 配置文件：[include/Log.hpp](include/Log.hpp)
- 实现文件：[src/Log.cpp](src/Log.cpp)
- 日志目录：`Log/`

## 更多信息

参考spdlog官方文档：https://github.com/gabime/spdlog
