# 实操清单（Checklist）

## 监听启动
- 创建 socket（非阻塞 + CLOEXEC）
- 设置：`REUSEADDR`，`REUSEPORT(可选)`,`TCP_NODELAY(可选)`,`KEEPALIVE(可选)`
- 解析并 `bind`（遍历 `addrinfo` 直到成功）
- `listen(backlog)`（一般用 `SOMAXCONN`）
- 构造 `Channel(fd)` 设置回调，`enableReading()`

## 接受新连接
- 循环 `accept` 至 `EAGAIN`；处理 `EINTR` 重试、`ECONNABORTED` 跳过
- 未设置新连接回调时关闭新 fd

## 读事件处理
- 循环 `read` 到 `EAGAIN`；`n==0` 判关闭，进入清理流程
- 错误：`EINTR` 重试；其他记录并关闭

## 写事件处理
- 写尽；剩余挂起 `EPOLLOUT`
- 发送完毕后 `disableWriting()`

## epoll 管理
- `events==0` → `DEL`，置 `added_=false` 并从 map 擦除
- 优先 `MOD`；不存在回退 `ADD`；`ADD` EEXIST 回退 `MOD`
- 仅在 `epoll_ctl` 成功后更新 map/状态位

## 关闭流程
- 从 epoll 移除（`remove()`/`DEL`）
- 关闭 fd
- 释放上层资源（缓冲、对象）
