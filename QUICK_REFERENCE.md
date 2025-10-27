# 快速参考卡 - Linux 网络编程

## 错误码快速判断表

```
读写错误处理决策树：
│
├─ return == -1?
│  │
│  ├─ YES → errno == ?
│  │  │
│  │  ├─ EINTR      → continue（重试）
│  │  ├─ EAGAIN     → break（等待）
│  │  ├─ EWOULDBLOCK→ break（等待）
│  │  ├─ EPIPE      → error（关闭）
│  │  └─ 其他        → error（关闭）
│  │
│  └─ NO → return == 0?
│     │
│     ├─ YES → EOF（关闭）
│     └─ NO  → 成功，继续
```

## Socket 编程检查清单

### 创建阶段
- [ ] socket() 检查返回 -1
- [ ] setsockopt(SO_REUSEADDR) 设置
- [ ] 确定阻塞/非阻塞模式

### 服务端准备
- [ ] bind() 检查返回值
- [ ] listen() 检查返回值
- [ ] 设置 listen socket 为非阻塞（可选）

### 事件处理
- [ ] accept4() 使用 SOCK_NONBLOCK | SOCK_CLOEXEC
- [ ] LT 模式用于 listen socket
- [ ] ET 模式用于 data socket
- [ ] ET 模式必须循环读到 EAGAIN

### 数据通信
- [ ] send() 检查返回值
- [ ] 处理部分发送（accumulate）
- [ ] recv() 检查返回 0（EOF）
- [ ] 错误处理：EINTR/EAGAIN 分离

### 关闭阶段
- [ ] 及时从 epoll 移除 fd
- [ ] close(fd) 检查返回值
- [ ] ⚠️ shared_ptr 生命周期管理（避免双重关闭）
  - release() → removeChannel() → close()
- [ ] RAII 自动清理

## 性能调优记住这些

| 操作 | 阻塞 | 非阻塞 | 推荐 |
|------|------|--------|------|
| 单个连接 | ✅ | ❌ | 阻塞 |
| 多个连接 | ❌ | ✅ | 非阻塞 |
| 监听连接 | ✅ | ✅ | LT |
| 数据通信 | ✅ | ✅ | ET |

## 常用系统调用

```
创建阶段：
  socket()     - 创建 socket
  setsockopt() - 设置选项
  fcntl()      - 设置 fd 属性
  
绑定阶段：
  bind()       - 绑定地址
  listen()     - 开始监听
  
连接阶段：
  connect()    - 主动连接
  accept4()    - 接受连接
  
通信阶段：
  send()       - 发送数据
  recv()       - 接收数据
  read()       - 同 recv()
  write()      - 同 send()
  
事件阶段：
  epoll_create()  - 创建 epoll
  epoll_ctl()     - 添加/删除 fd
  epoll_wait()    - 等待事件
  
关闭阶段：
  close()      - 关闭 fd
  shutdown()   - 关闭连接（优雅）
```

## 一行代码记住关键概念

| 概念 | 记忆 |
|------|------|
| EINTR | 被中断，重试 |
| EAGAIN | 资源不可用，等待 |
| EPIPE | 对端已关，别写了 |
| LT | 级别触发，多次通知 |
| ET | 边界触发，一次通知 |
| SOCK_NONBLOCK | 创建非阻塞 socket |
| SOCK_CLOEXEC | fork 后自动关闭 |

## 如何调试网络程序

```bash
# 查看 socket 状态
ss -tlnp

# 查看进程打开的 fd
lsof -p <pid>

# 抓包查看网络流量
tcpdump -i lo port 8090

# 查看 TCP 统计
netstat -s

# strace 追踪系统调用
strace -e trace=network,read,write ./server
```

## 常见问题速查

**Q: 为什么 read() 返回 -1？**
A: 检查 errno
- EAGAIN → 等待下次 epoll
- EINTR → 重试
- 其他 → 真错误，关闭

**Q: ET 模式漏了数据？**
A: 没有循环读到 EAGAIN
```cpp
while (read(...) > 0) { }  // ← 必须加 while
```

**Q: send() 返回小于发送大小？**
A: 正常！部分发送，继续发剩余部分
```cpp
total += sent;  // 累计，不能忽略返回值
```

**Q: 连接卡住不动？**
A: 可能是
- fd 设置有误（阻塞/非阻塞）
- epoll 没有正确注册
- 没有循环等待（ET 模式）

**Q: CPU 占用 100%？**
A: 忙轮询！
- continue EAGAIN → ❌
- return EAGAIN → ✅

**Q: 端口被占用？**
A: 设置 SO_REUSEADDR
```cpp
int yes = 1;
setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
```

**Q: 客户端收不到回显？**
A: 检查
- send() 是否返回成功
- 接收端是否调用了 recv
- 非阻塞时是否需要等待

**Q: 收到一条消息后连接就断了？**
A: 检查 `recv()` 对 `EAGAIN` 的处理
```cpp
// ❌ 错误：把 EAGAIN 当成 0 返回，上一层会当成 EOF
if (errno == EAGAIN) return 0;

// ✅ 正确：返回 -1，让 errno 保留 EAGAIN
if (errno == EAGAIN) return -1;
```

**Q: close 报错 "Bad file descriptor"？**
A: shared_ptr 生命周期问题（双重关闭）
```cpp
// ❌ 错误顺序
loop.removeChannel(fd);  // Channel 析构，lambda 销毁，conn 引用计数-1
close(fd);               // 第二次关闭失败

// ✅ 正确顺序
conn.release();          // 先释放 fd 所有权
loop.removeChannel(fd);  // Channel 析构，但 TcpStream 不会关闭 fd
close(fd);               // 正常关闭
```

---

## 代码模板

### ET 模式完整处理

```cpp
while (true) {
    ssize_t n = read(fd, buf, BUF_SIZE);
    
    if (n > 0) {
        // 处理 n 字节数据
        handle(buf, n);
    } else if (n == 0) {
        // 对端关闭
        return;
    } else {
        // n == -1
        if (errno == EINTR) continue;
        if (errno == EAGAIN) break;  // 读完了
        // 真错误
        perror("read");
        return;
    }
}
```

### 发送完整数据

```cpp
size_t total = 0;
while (total < size) {
    ssize_t sent = send(fd, data + total, size - total, 0);
    
    if (sent > 0) {
        total += sent;
    } else if (sent == -1) {
        if (errno == EINTR) continue;
        if (errno == EAGAIN) break;  // 缓冲满，等下次可写
        // 真错误
        return -1;
    }
}
```

---

这份卡片快速参考，遇到问题时可以直接查阅！
