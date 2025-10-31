# Linux 网络编程学习笔记 - 高性能 Echo 服务器

## 目录
1. [项目架构](#项目架构)
2. [架构设计](#架构设计)
3. [核心概念](#核心概念)
4. [设计模式](#设计模式)
5. [常见错误](#常见错误)
6. [深入理解](#深入理解)
7. [最佳实践](#最佳实践)

---

## 项目架构

### 整体设计

```
┌─────────────────────────────────────────────┐
│           Main EventLoop                     │
│  (监听客户端连接和数据)                      │
└──────────────┬──────────────────────────────┘
               │
      ┌────────┴────────┐
      │                 │
      v                 v
┌──────────────┐  ┌──────────────────┐
│ Listen       │  │ Client Data      │
│ Connection   │  │ (ET模式)         │
│ (LT模式)     │  │                  │
└──────────────┘  └──────────────────┘
      │                 │
      │                 ├─→ handleConnection
      │                 │
      └─→handleNew      └─→TcpStream
         Connection
```

### 类体系

```
SocketBase (基类：fd 管理)
├── ClientSocket (客户端连接)
├── ServerSocket (服务端监听)
└── TcpStream (数据传输)

Channel (事件处理)
└─→ 由 EventLoop 管理

EventLoop (事件循环)
└─→ 使用 EpollPoller
```

---

## 架构设计

### 为什么这个设计这么复杂？

这是异步事件驱动架构的**必然复杂性**，不是设计问题。

#### 同步 vs 异步

```cpp
// ❌ 同步代码（简单）
void processConnection(Socket s) {
    data = read(s);        // 等待
    handle(data);
    close(s);              // 立即关闭
}  // 生命周期 = 函数执行周期

// ✅ 异步代码（复杂）
void processAsync(Socket s) {
    epoll_add(s);          // 注册
    return;                // 对象什么时候关闭？
}  // 生命周期 > 函数执行周期
```

**关键差异：**
- 同步：对象生命周期 = 函数执行周期
- 异步：对象生命周期 > 函数执行周期

#### 多所有权问题

你的代码中，一个 `TcpStream` 对象被多个地方持有：

```cpp
auto conn = std::make_shared<TcpStream>(...);  // 所有者 #1
channel->setReadCallback([conn]() {            // 所有者 #2（lambda）
    handleConnection(loop, *conn);
});
loop.addChannel(std::move(channel));           // 所有者 #3（EventLoop）
```

**问题：谁负责关闭 fd？**
- 如果 conn 超出作用域立即关闭 → lambda 还在用，崩溃
- 如果 lambda 关闭 → EventLoop 还在用，崩溃
- 如果 EventLoop 关闭 → 其他地方还在用，崩溃

**解决方案：**
```cpp
// 明确所有权转移
conn.release();           // ← conn 不再拥有 fd
loop.removeChannel(fd);   // ← EventLoop 放弃 fd
close(fd);                // ← 我们显式关闭
```

### 对标行业做法

**Boost.Asio（C++ 异步库）：**
```cpp
// 必须使用 shared_ptr
void async_read(
    std::shared_ptr<socket_type> sock,  // ← 强制 shared_ptr
    ...
)
```

**libuv（Node.js 底层）：**
```c
// 使用引用计数标记
struct uv_handle_s {
    unsigned int flags;  // ← 标记关闭状态
};

void uv_close(uv_handle_t* handle, ...) {
    handle->flags |= UV_HANDLE_CLOSING;  // ← 标记为关闭中
}
```

**Redis：**
```cpp
void closeClient(Client* c) {
    aeDeleteFileEvent(...);  // 移除事件
    close(c->fd);            // 关闭 fd
    free(c);                 // 释放内存
}
```

**结论：你的做法与行业标准一致。**

### 为什么用 shared_ptr？

三个条件同时存在时必须用 shared_ptr：

1. **异步性**：对象被异步访问（Lambda 稍后被调用）
2. **共享性**：多个地方持有对象引用
3. **不可控**：销毁时机由 EventLoop 决定

```
异步 + 共享 + 不可控
    ↓
需要自动生命周期管理
    ↓
shared_ptr 是唯一选择
```

详见：**SMART_POINTER_GUIDE.md**

---

## 核心概念

### 1. 非阻塞 Socket vs 阻塞 Socket

#### 阻塞 Socket（Blocking）
```cpp
// 默认模式
ClientSocket client;  // ← 阻塞
TcpStream conn = client.connectTo(addr);
auto data = conn.recvString();  // ← 会等待数据
```

**特点：**
- 调用会一直等待到操作完成
- 代码简单直观
- 适合简单客户端程序
- 多连接时容易阻塞

#### 非阻塞 Socket（Non-blocking）
```cpp
// 需要配合事件循环
ServerSocket server;  // ← 非阻塞
// 如果数据还没到达，立即返回 -1，errno 为 EAGAIN
```

**特点：**
- 调用立即返回
- 需要配合 epoll/poll/select
- 适合高性能服务器
- 代码复杂，需要事件驱动

### 2. LT 模式 vs ET 模式

#### LT（Level Triggered）- 水平触发
```cpp
// 只要有数据就触发
listenChannel->setInterestedEvents(EPOLLIN);  // ← LT 模式
```

**行为：**
- 只要 fd 就绪，epoll_wait 就返回
- 可以多次触发同一事件
- 不需要一次读完所有数据

**何时使用：**
- 监听 socket（accept）
- 防止错过连接

#### ET（Edge Triggered）- 边界触发
```cpp
// 只在状态变化时触发一次
connChannel->setInterestedEvents(EPOLLIN | EPOLLET);  // ← ET 模式
```

**行为：**
- 只在 fd 状态变化时触发一次
- 必须一次读完所有数据，否则错过
- 需要循环读直到 EAGAIN

**何时使用：**
- 数据连接（ET 更高效）
- 需要高性能场景

**重要：必须循环读！**
```cpp
while (true) {
    n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        process(buf, n);
    } else if (errno == EAGAIN) {
        break;  // ← 必须检查，读完了
    }
}
```

### 3. 错误码理解

#### EINTR（中断系统调用）
```cpp
if (errno == EINTR) {
    continue;  // ← 被信号打断，安全重试
}
```
- 系统调用被信号处理程序中断
- **应该重试** - 这不是真错误

#### EAGAIN / EWOULDBLOCK（非阻塞没有数据）
```cpp
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return;  // ← 等待下次 epoll 通知
}
```
- 非阻塞 socket 数据不可用
- **不应该重试** - 会造成忙轮询
- 应该等待 epoll 通知

#### 真错误
```cpp
if (errno == EPIPE || errno == ECONNRESET) {
    closeConnection();  // ← 关闭连接
}
```
- 连接已关闭、内存错误等
- **应该返回错误或关闭连接**

### 4. 缓冲区和发送失败处理

#### 为什么 send() 会失败？

```
Client          Network Buffer        Server
  │                  (Full!)              │
  │─────→ data ─────→ [████████████]      │
         errno=EAGAIN
         返回 -1
```

**原因：**
- 内核发送缓冲区满（接收方处理不及时）
- TCP 流控触发

**你的实现：**
```cpp
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return false;  // ← 返回 false，让 epoll 在可写时重试
}
```

---

## 设计模式

### 1. RAII（资源获取即初始化）

```cpp
// 构造时打开，析构时关闭
class SocketBase {
public:
    SocketBase(bool nonblock = false) {
        socket_ = socket(...);  // 获取资源
        if (nonblock) setNonBlock();
    }
    
    ~SocketBase() {
        if (socket_ != -1)
            close(socket_);  // 释放资源
    }
};

// 使用者无需手动管理
{
    ServerSocket sock;  // 自动打开
}  // 自动关闭
```

**优点：**
- 不会泄漏 fd
- 异常安全
- 代码简洁

### 2. 回调模式

```cpp
Channel ch(fd);
ch.setReadCallback([]() {
    // epoll 触发时调用
    handleConnection();
});
```

**优点：**
- 解耦事件和处理
- 易于测试
- 支持多种事件处理方式

### 3. 异常处理

```cpp
// 好的做法：明确区分错误
try {
    auto conn = listenSock.acceptConnection();
    // ...
} catch (const std::exception& ex) {
    if (std::string(ex.what()).find("no pending connection") != std::string::npos) {
        // 预期的：没有连接（ET 模式）
        return;
    }
    // 真错误
    std::cerr << "accept failed: " << ex.what() << std::endl;
}
```

---

## 常见错误

### 1. ❌ 非阻塞 socket 直接 continue

**错误代码：**
```cpp
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    continue;  // ❌ 错！会忙轮询 CPU 100%
}
```

**为什么错：**
- 没有数据时，循环会不停地尝试读
- CPU 会一直旋转
- 浪费资源

**正确做法：**
```cpp
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return;  // ✅ 返回，等待下次 epoll 通知
}
```

### 2. ❌ ET 模式没有循环读

**错误代码：**
```cpp
// ET 模式
n = read(fd, buf, sizeof(buf));
if (n > 0) {
    process(buf, n);
    return;  // ❌ 读完了一次，但可能还有数据！
}
```

**为什么错：**
- ET 只在状态变化时触发一次
- 如果只读一次，剩余数据会被错过
- epoll 不会再通知（因为 fd 还是可读的，只是状态没变）

**正确做法：**
```cpp
while (true) {
    n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        process(buf, n);
        // 继续读
    } else if (errno == EAGAIN) {
        break;  // ✅ 读完了
    }
}
```

### 3. ❌ 递归处理 EINTR

**错误代码：**
```cpp
ssize_t TcpStream::recvRaw(char* buff, size_t len) {
    ssize_t n = recv(...);
    if (n == -1 && errno == EINTR) {
        return recvRaw(buff, len);  // ❌ 递归，多次中断会爆栈
    }
    return n;
}
```

**为什么错：**
- 信号频繁时，递归深度会很深
- 最终导致栈溢出

**正确做法：**
```cpp
ssize_t TcpStream::recvRaw(char* buff, size_t len) {
    while (true) {  // ✅ 用循环替代递归
        ssize_t n = recv(...);
        if (n == -1 && errno == EINTR) {
            continue;
        }
        return n;
    }
}
```

### 4. ❌ 忘记检查 send() 返回值

**错误代码：**
```cpp
send(fd, data, size, 0);  // ❌ 返回值 -1 被忽略了
// 可能数据没发送，程序还以为发了
```

**正确做法：**
```cpp
ssize_t sent = send(fd, data, size, 0);
if (sent == -1) {
    if (errno == EAGAIN) {
        return false;  // 缓冲区满，稍后重试
    }
    return false;  // 真错误
}
// 可能只发了部分，需要继续发剩余部分
total_send += sent;
```

### 5. ❌ 混淆 accept 返回值

**错误代码：**
```cpp
int connfd = accept(listenfd, ...);
if (connfd == -1) {
    // 处理
}
// 但 listen socket 本身的 fd 仍有效！
// 不要 close(listenfd)
```

**要点：**
- `accept()` 返回**新的 fd**（连接 socket）
- 原 listen socket fd 保持不变
- 不要混淆两者

### 6. ❌ shared_ptr 生命周期导致的双重关闭

**问题现象：** 
```
[server] close fd 5 failed: Bad file descriptor
```
客户端只发送一次数据，服务器就报错"Bad file descriptor"。

**错误代码流程：**
```cpp
// 在 handleNewConnection 中
auto conn = std::make_shared<TcpStream>(listenSock.acceptConnection());
auto connChannel = std::make_shared<Channel>(connfd);

// lambda 捕获了 conn
connChannel->setReadCallback([conn, &loop]() {
    handleConnection(loop, *conn);
});

loop.addChannel(std::move(connChannel));
```

**错误在 closeConnection 中：**
```cpp
void closeConnection(EventLoop &loop, TcpStream& conn) {
    int fd = conn.getFd();
    
    loop.removeChannel(fd);  // ❌ 问题在这里！
    // removeChannel 会导致：
    // 1. Channel 被从 map 中删除
    // 2. Channel 析构
    // 3. lambda 也析构
    // 4. lambda 中的 conn shared_ptr 也析构
    // 5. conn 引用计数减 1
    // 6. 如果是最后一个引用，TcpStream 析构
    // 7. SocketBase::~SocketBase() 调用 close(fd) ← 第一次关闭
    
    if (close(fd) == -1) {  // ❌ 第二次关闭，fd 已经无效了
        // "Bad file descriptor" 错误
    }
    conn.release();  // 这时已经太晚了
}
```

**执行流程图：**
```
closeConnection()
  ├─ loop.removeChannel(fd)
  │   ├─ channels_.erase(it)  
  │   │   ├─ shared_ptr<Channel> 被删除
  │   │   ├─ Channel 析构
  │   │   ├─ lambda 析构
  │   │   ├─ conn shared_ptr 引用计数 -1
  │   │   └─ TcpStream 析构
  │   │       └─ SocketBase::~SocketBase()
  │   │           └─ close(fd)  ✅ 第一次关闭成功
  │   └─ 函数返回
  │
  └─ close(fd)  ❌ 第二次关闭失败
```

**为什么错：**
- `removeChannel` 删除 `shared_ptr<Channel>`，导致 Channel 析构
- lambda 被销毁时，其捕获的变量也被销毁
- `conn` 的引用计数减 1，如果是最后一个引用则析构
- TcpStream 析构时会在 `~SocketBase()` 中调用 `close(fd)`
- 接着我们再调用 `close(fd)` 就会失败

**正确做法：**
```cpp
void closeConnection(EventLoop &loop, TcpStream& conn) {
    int fd = conn.getFd();
    
    // 1️⃣ 第一步：释放 fd 所有权
    //    这样当 TcpStream 析构时不会再 close
    conn.release();
    
    // 2️⃣ 第二步：移除 Channel（此时析构不会关闭 fd）
    loop.removeChannel(fd);
    
    // 3️⃣ 第三步：我们来关闭 fd
    if (close(fd) == -1) {
        std::cerr << "[server] close fd " << fd << " failed: " 
                  << strerror(errno) << std::endl;
    }
}
```

**关键要点：**
1. **release() 必须在 removeChannel() 之前**
2. 这样当 lambda 析构导致 conn 引用计数减 1 时，fd 已经被 release 了
3. TcpStream 析构时不会 close，避免双重关闭
4. 我们在 removeChannel 后手动 close

**记住这个顺序：**
```
release() → removeChannel() → close()
```

### 7. ❌ 把 EAGAIN 当成 EOF

**问题现象：**
```
客户端只发送一次消息，服务器立即关闭连接
```

**错误代码：**
```cpp
ssize_t TcpStream::recvRaw(char* buff, size_t len) {
    ssize_t recv_byte = recv(socket_, buff, len, 0);
    if (recv_byte == -1) {
        if (errno == EINTR) return recvRaw(buff, len);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // ❌ 这里把 EAGAIN 当成 EOF
        }
        return -1;
    }
    return recv_byte;
}
```

**为什么错：**
- `recv()` 返回 **0** 才代表对端发送 FIN（连接关闭）
- `EAGAIN/EWOULDBLOCK` 只是“暂时没有数据”，连接仍然有效
- 返回 0 会让上层逻辑误以为对端关闭，从而调用 `close()`
- 结果：服务器在收到第一条消息后就主动断开连接

**正确做法：**
```cpp
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return -1;           // 让上层看到 errno == EAGAIN
}
```
```cpp
// handleConnection 中：
if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;         // 暂时没有更多数据，保持连接
    }
    closeConnection(...);
}
```

**关键要点：**
1. `recv()` 返回 0 = 对端关闭；`EAGAIN` ≠ 0
2. 非阻塞 socket 要保留 `errno` 让外层判断
3. ET 模式下，遇到 `EAGAIN` 表示“数据读完了”，但连接还在

---

## 深入理解

### 1. Socket 的生命周期

```
┌─────────────┐
│   创建      │ socket() 返回 fd
├─────────────┤
│   绑定      │ bind() - 仅服务端
├─────────────┤
│   监听      │ listen() - 仅服务端
├─────────────┤
│   连接      │ connect() - 仅客户端
├─────────────┤
│   通信      │ send() / recv()
├─────────────┤
│   关闭      │ close() 释放 fd
└─────────────┘
```

### 2. TCP 连接建立过程（Three-Way Handshake）

```
Client                          Server
  │                               │
  │─────1. SYN──────────────→      │
  │                         ├─ 收到 SYN
  │        ←─────2. SYN-ACK─       │
  ├─ 收到 SYN-ACK                 │
  │─────3. ACK──────────────→      │
  │                         ├─ 收到 ACK
  │                               │
  └─── 连接建立 ───────────────────┘
```

**你的代码中：**
```cpp
// 客户端
TcpStream conn = client.connectTo(addr);  // ← 发送 SYN，等待回复

// 服务端
auto conn = listenSock.acceptConnection();  // ← 等待完整连接
```

### 3. epoll 的工作流程

```
1. epoll_create() - 创建 epoll 对象
   │
2. epoll_ctl(EPOLL_CTL_ADD, ...) - 注册感兴趣的 fd
   │
   ├─→ listen_fd (EPOLLIN, LT)
   └─→ conn_fd (EPOLLIN | EPOLLET, ET)
   │
3. epoll_wait() - 等待事件
   │
   ├─ listen_fd 有新连接 → 触发 handleNewConnection()
   │
   ├─ conn_fd 有数据 → 触发 handleConnection()
   │  ├─ 读数据
   │  ├─ 处理数据
   │  └─ 发回数据
   │
4. 循环回到 3
```

### 4. 为什么需要 CLOEXEC？

```cpp
int connfd = accept4(listenfd, ..., SOCK_CLOEXEC);
```

**场景：**
```cpp
// 主进程
fork();  // 创建子进程
// 子进程会继承 connfd！
// 如果没有 CLOEXEC，exec() 后 connfd 仍打开
// 导致连接泄漏
```

**CLOEXEC 的作用：**
```cpp
// 用 CLOEXEC 标记 fd
if (exec*())  // 执行新程序
    // fd 自动关闭，不会被继承
```

---

## 最佳实践

### 1. 错误处理的完整流程

```cpp
// ✅ 好的做法
ssize_t n = read(fd, buf, sizeof(buf));
if (n > 0) {
    // 有数据，处理
    process(buf, n);
} else if (n == 0) {
    // 对端关闭
    closeConnection();
} else {
    // n < 0，errno 需要检查
    if (errno == EINTR) {
        // 被信号打断，重试
        return retryRead(fd, buf);
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 非阻塞，没数据，等待
        return;
    }
    // 真错误
    logError("read failed");
    closeConnection();
}
```

### 2. 性能优化

```cpp
// ✅ 使用 accept4() 一次性设置
int fd = accept4(listen_fd, addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
// 而不是：
int fd = accept(listen_fd, addr, &len);
fcntl(fd, F_SETFL, O_NONBLOCK);  // ❌ 多次系统调用
fcntl(fd, F_SETFD, FD_CLOEXEC);  // ❌ 浪费
```

### 3. 日志调试

```cpp
// ✅ 带上下文信息的日志
std::cerr << "[server] new connection fd=" << connfd 
          << " from " << peer_ip << ":" << peer_port << std::endl;

std::cerr << "[server] read " << n << " bytes from fd=" << fd << std::endl;

if (errno != 0) {
    std::cerr << "[error] " << strerror(errno) << std::endl;
}
```

### 4. 防御性编程

```cpp
// ✅ 总是检查返回值
if (close(fd) == -1) {
    std::cerr << "close failed: " << strerror(errno) << std::endl;
}

// ✅ 检查指针有效性
if (data.empty()) {
    return false;
}

// ✅ 验证 fd 有效性
if (fd < 0) {
    return;
}
```

### 5. 资源清理

```cpp
// ✅ 不要手动 close，让 RAII 处理
{
    ServerSocket sock;  // 自动打开
    sock.bindTo(addr);
    sock.startListening();
}  // 自动关闭

// ✅ 使用 shared_ptr 避免内存泄漏
auto conn = std::make_shared<TcpStream>(fd);
// conn 超出作用域时自动释放
```

---

## 总结

### 关键要点

1. **阻塞 vs 非阻塞**
   - 客户端通常阻塞（简单）
   - 服务端通常非阻塞 (高效)

2. **LT vs ET**
   - 监听 socket 用 LT（防止错过连接）
   - 数据 socket 用 ET（高性能）

3. **错误处理**
   - EINTR → 重试
   - EAGAIN → 等待
   - 其他 → 关闭连接

4. **ET 模式必须循环读**
   - 一次读完所有数据
   - 检查 EAGAIN 才算读完

5. **资源管理**
   - 使用 RAII
   - 使用 shared_ptr
   - 让析构函数自动清理

### 进一步学习方向

- [ ] 学习 TCP 四次挥手（关闭连接）
- [ ] 研究半关闭状态（shutdown）
- [ ] 了解 TCP 粘包问题
- [ ] 学习协议设计
- [ ] 研究线程安全（如果涉及多线程）
- [ ] 性能优化（如零拷贝、mmap 等）

---

## 参考代码片段

### 完整的 ET 模式读写

```cpp
// 读取（ET 模式）
while (true) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        processData(buf, n);
    } else if (n == 0) {
        closeConnection();
        return;
    } else {
        if (errno == EINTR) continue;
        if (errno == EAGAIN) break;  // 读完了
        closeConnection();
        return;
    }
}

// 写入（处理部分发送）
size_t total = 0;
while (total < size) {
    ssize_t sent = send(fd, data + total, size - total, 0);
    if (sent > 0) {
        total += sent;
    } else if (sent == -1) {
        if (errno == EINTR) continue;
        if (errno == EAGAIN) break;  // 缓冲区满
        return false;
    }
}
```

---

**文档最后更新：2025-10-28**

---

## 错误汇总表

| # | 错误 | 症状 | 原因 | 解决 |
|---|------|------|------|------|
| 1 | EAGAIN continue | CPU 100% | 忙轮询 | return，等待 epoll |
| 2 | ET 模式单次读 | 数据丢失 | 只读一次未读完 | while 循环读到 EAGAIN |
| 3 | EINTR 递归处理 | 栈溢出 | 中断频繁递归深 | 改用 while 循环 |
| 4 | 忽略 send 返回 | 数据丢失 | 可能部分发送 | 检查返回值，累计发送 |
| 5 | accept fd 混淆 | 关闭错误 fd | 混淆 listen 和 conn | accept 返回新 fd |
| 6 | shared_ptr 双重关闭 | Bad file descriptor | removeChannel 导致析构 | release() → remove → close() |
| 7 | 把 EAGAIN 当成 EOF | 收一次就断开 | recvRaw 返回 0 | 把 EAGAIN 原样返回，外层判断 |

### 详细说明

**错误 1: EAGAIN 的 continue vs return**
- ❌ `continue` 会立即重试，导致忙轮询
- ✅ `return` 等待 epoll，让 fd 准备好时再通知

**错误 2: ET 模式必须循环读**
- ❌ 读一次就返回，剩余数据永远不会通知
- ✅ 用 while 循环读到 EAGAIN 才返回

**错误 3: EINTR 递归风险**
- ❌ 信号频繁时递归会很深，最终栈溢出
- ✅ 改用 while 循环，可处理任意次数信号

**错误 4: send 返回值检查**
- ❌ 忽略返回值，以为全部发送
- ✅ 检查返回值，可能是部分发送或错误

**错误 5: accept 返回值**
- ❌ 混淆 listen fd 和 client fd
- ✅ accept 返回新 fd，listen fd 保持不变

**错误 6: shared_ptr 生命周期**
- ❌ removeChannel 导致 Channel 析构，lambda 销毁，引用计数-1，导致双重关闭
- ✅ release() 先释放 fd，再 removeChannel，最后 close

**错误 7: EAGAIN ≠ EOF**
- ❌ 遇到 EAGAIN 返回 0，handleConnection 误判连接关闭
- ✅ 保留 errno 返回 -1，让外层看到 EAGAIN 并继续保持连接

---

**最后更新：2025-10-28**
