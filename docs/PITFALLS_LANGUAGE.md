# 语言层面坑点与最佳实践（C++/POSIX）

适用范围：当前代码库（不含 `src/old`）。

## errno 与错误处理
- 仅在调用失败后读取；成功不会清零。立刻保存：`int e = errno;`，避免后续调用覆盖。
- 文本化优先 `strerror_r` 或 `std::error_code/system_error`，`getaddrinfo` 用 `gai_strerror(ret)`。
- 常见错误：
  ### 模式示例：安全读取 errno 与构造 system_error
  ```cpp
  #include <cerrno>
  #include <system_error>

  int fd = ::accept4(lfd, nullptr, nullptr, SOCK_NONBLOCK|SOCK_CLOEXEC);
  if (fd == -1) {
    int e = errno; // 立刻保存
    if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) {
      // 可恢复
    } else {
      throw std::system_error(e, std::generic_category(), "accept4 failed");
    }
  }
  ```
  - `EAGAIN/EWOULDBLOCK`: 非阻塞暂不可完成 → 稍后重试。
  - `EINTR`: 被信号中断 → 重试。
  - `EINPROGRESS`: 非阻塞 `connect` 进行中 → 等 `EPOLLOUT` 后 `getsockopt(SO_ERROR)` 判定。
  - `ECONNABORTED/ECONNRESET/ETIMEDOUT/EPIPE`：连接异常/超时/对端关闭。

## 非阻塞 I/O 基本律
### 读循环（至 EAGAIN）
```cpp
for (;;) {
  ssize_t n = ::read(fd, buf, sizeof(buf));
  if (n > 0) { /* 处理 */ }
  else if (n == 0) { /* 对端关闭：移除+close */ break; }
  else {
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
    /* 其他错误：记录并关闭 */ break;
  }
}
```

### 写循环（挂起 EPOLLOUT）
```cpp
while (!outBuf.empty()) {
  ssize_t n = ::write(fd, outBuf.data(), outBuf.size());
  if (n > 0) outBuf.erase(0, n);
  else if (n == -1 && errno == EINTR) continue;
  else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    ch.enableWriting(); // 等可写事件
    break;
  } else { /* 错误：EPIPE/ECONNRESET等 → 关闭 */ break; }
}
if (outBuf.empty()) ch.disableWriting();
```
- 套接字全链路非阻塞：`accept4(..., SOCK_NONBLOCK|SOCK_CLOEXEC)` 或 `fcntl(O_NONBLOCK)`。
- 读：循环 `read` 到 `-1&&EAGAIN`；`0` 判对端关闭。
- 写：尽量写尽，剩余挂起 `EPOLLOUT`，写完后 `disableWriting()`。
- `accept`：循环到 `EAGAIN`；`EINTR` 重试；`ECONNABORTED` 忽略继续。

## 地址解析与 sockaddr 使用
### 从 sockaddr 直接构造 InetAddress（避免二次解析）
```cpp
sockaddr_storage ss; socklen_t len = sizeof(ss);
int cfd = ::accept4(lfd, reinterpret_cast<sockaddr*>(&ss), &len, SOCK_NONBLOCK|SOCK_CLOEXEC);
if (cfd >= 0) {
  Server::InetAddress peer{reinterpret_cast<sockaddr*>(&ss), len};
  // 直接使用 peer.addr()/peer.addrlen()
}
```
- `getaddrinfo` 阻塞；服务启动可接受，数据通路谨慎使用。
- 已有 `sockaddr`（如 `accept` 返回）时，直接使用，不要二次解析。
- `InetAddress` 现支持从 `sockaddr` 构造，`addr()/addrlen()` 无缝返回内部存储。

## 套接字选项要点
### 忽略 SIGPIPE 与 MSG_NOSIGNAL
```c
// 进程级屏蔽 SIGPIPE（推荐）
struct sigaction sa = {0};
sa.sa_handler = SIG_IGN; sigaction(SIGPIPE, &sa, NULL);
```
```cpp
// 或在发送时使用 MSG_NOSIGNAL（可选）
::send(fd, data, len, MSG_NOSIGNAL);
```
- `SO_REUSEADDR`：服务重启常用；`SO_REUSEPORT` 视内核支持，容错处理 `ENOPROTOOPT/EINVAL`。
- `TCP_NODELAY`：可降延迟；`SO_KEEPALIVE` 需配合 TCP_KEEP* 参数。
- `CLOEXEC`：避免 fd 跨 exec 泄漏。
- Linux 屏蔽 `SIGPIPE` 或发送时用 `MSG_NOSIGNAL` 以免进程被信号终止。

## C++ 资源与异常
### 移动语义示例
```cpp
Socket::Socket(Socket&& o) noexcept : socketfd_(o.socketfd_) { o.socketfd_ = -1; }
Socket& Socket::operator=(Socket&& o) noexcept {
  if (this != &o) { if (socketfd_!=-1) ::close(socketfd_); socketfd_=o.socketfd_; o.socketfd_=-1; }
  return *this;
}
```
- 析构函数不得抛异常；当前析构仅 `close/freeaddrinfo`。
- 移动语义：转移所有权后源对象置“空”（fd=-1/res_=nullptr）。
- 头文件最小化：前置声明 + 在 .cpp 包含系统头，降低编译耦合。
