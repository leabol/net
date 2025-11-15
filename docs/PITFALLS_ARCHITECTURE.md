# 架构与运行机制（epoll/事件循环）

## 事件模型
- 监听与数据通路：监听 fd 用 LT 更稳；数据连接可用 ET，但必须“读/写至 EAGAIN”。
- 事件集：`EPOLLIN/EPOLLOUT/EPOLLERR/EPOLLHUP/EPOLLRDHUP` 常用；`EPOLLPRI` 较少用。
- 回调准则：
  - 读：循环直到 `EAGAIN`；`0` 判关闭。
  - 写：写尽，剩余挂起 `EPOLLOUT`；写空后及时取消写关注。
  - 关闭：从 epoll 删除，再关闭 fd。

## epoll_ctl 心智模型
- ADD 仅在不存在时成功（否则 `EEXIST`）。
- MOD 仅在存在时成功（不存在多为 `ENOENT`/`EINVAL`）。
- DEL 仅在存在时成功（不存在 `ENOENT`）。
- 容错：采用“MOD 优先，ENOENT→ADD；ADD 遇 EEXIST→MOD”。

## 连接管理
### 非阻塞 connect 全流程（EPOLLOUT + SO_ERROR）
```cpp
Socket s; s.setNonblock();
int r = ::connect(s.fd(), addr, addrlen);
if (r == 0) { /* 直连成功（如回环） */ }
else if (r == -1 && errno == EINPROGRESS) {
  Channel ch{&loop, s.fd()};
  ch.setWriteCallback([&]() {
    int err=0; socklen_t elen=sizeof(err);
    ::getsockopt(s.fd(), SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err == 0) {
      ch.disableWriting(); /* 连接建立，转入读写 */
    } else {
      // 连接失败：记录 err，清理并关闭
    }
  });
  ch.enableWriting(); // 等可写即连接完成（或失败）
} else {
  // 立即失败：记录 errno 并退出
}
```
- accept 并发风暴：循环到 `EAGAIN`，避免漏接；如使用多线程/多进程监听，`SO_REUSEPORT` 助于均衡（注意内核版本差异）。
- 半关闭：`EPOLLRDHUP` 可检测对端关闭写；按需触发应用层关闭逻辑。
- 背压与缓冲：写路径需要用户态缓冲管理（本仓库 demo 为简化版）。

## 信号/系统层面
- `SIGPIPE`：对端关闭写时发送，建议忽略或 `MSG_NOSIGNAL`。
- backlog：`listen(backlog)` 受内核上限，过大无效；`SOMAXCONN` 一般足够。
- `AI_ADDRCONFIG`：容器或无全局地址环境可能返回空集，需要可配置关闭。
