# 为什么要用 shared_ptr？详细指南

## 问题背景

你可能想问：为什么不直接这样做？

```cpp
// ❌ 这样不行吗？
TcpStream conn(listenSock.acceptConnection());
Channel connChannel(connfd);
```

答案是：在异步事件驱动的架构中，这样**非常危险**。让我来逐个解释。

---

## 核心问题：生命周期不匹配

### 场景 1: Lambda 捕获对象

```cpp
// ❌ 错误的做法
TcpStream conn = listenSock.acceptConnection();
Channel channel(connfd);

channel.setReadCallback([&conn, &loop]() {  // 引用捕获
    handleConnection(loop, conn);
});

loop.addChannel(std::move(channel));
// <- 这里 conn 和 channel 超出作用域，被销毁！
// 但 lambda 还在 EventLoop 中等待被调用
// 当 epoll 触发时，lambda 会访问已销毁的对象！
```

**执行流程：**
```
1. conn 创建 ✅
2. channel 创建 ✅
3. lambda 使用引用捕获 conn 和 channel
4. addChannel(channel) - channel 移到 EventLoop
5. 超出作用域 ❌
   ├─ conn 析构（fd 被关闭）
   └─ channel 如果是值，也会析构
6. epoll_wait() 返回事件
7. lambda 被调用
8. 访问已销毁的 conn ❌❌❌ 段错误！
```

### 为什么引用捕获不安全？

```cpp
[&conn]()  // 引用捕获
// 这只是保存了 &conn 的地址
// 当 conn 被销毁后，这个地址指向的是已释放的内存
// lambda 调用时就会访问野指针！
```

---

## 解决方案：shared_ptr

### 用 shared_ptr 如何解决

```cpp
// ✅ 正确的做法
auto conn = std::make_shared<TcpStream>(...);  // 引用计数 = 1
auto channel = std::make_shared<Channel>(...); // 引用计数 = 1

channel->setReadCallback([conn, &loop]() {     // 值捕获！引用计数 = 2
    handleConnection(loop, *conn);
});

loop.addChannel(std::move(channel));           // channel 移到 EventLoop
// <- 超出作用域

// 此时：
// - conn 的引用计数 = 2（main 作用域 + lambda）
// - channel 的引用计数 = 2（loop + 语句块）
// conn 不会被销毁！
// 
// 当 epoll 触发时：
// 1. lambda 被调用
// 2. conn 仍然有效！
// 3. handleConnection 安全执行
```

**引用计数追踪：**
```
创建时：
  conn: 1 (局部变量)

lambda 捕获时：
  conn: 2 (局部变量 + lambda)

addChannel 时：
  conn: 2 (lambda + EventLoop 中的 Channel)

超出作用域时：
  conn: 2 → 1 (局部变量析构)
  channel: 1 (EventLoop 中)

lambda 被调用时：
  conn: 1 (lambda 还活着)
  ✅ conn 可以安全使用！
```

---

## 问题的具体场景

### 情况 A: 异步回调（需要 shared_ptr）

```cpp
// EventLoop 在主线程运行
// handleConnection 在回调中运行（稍后）

loop.addChannel(...);  // 注册

// 主线程继续执行，conn 超出作用域
// ... 

// 稍后，epoll 触发，handleConnection 被调用
// 如果用的是引用或裸指针，此时访问会出错！
```

**这是网络服务器的典型模式：**
```
时刻 T0: accept 连接，注册到 epoll
时刻 T0+100ms: epoll 检测到数据到达
时刻 T0+100ms: 调用 handleConnection
```

中间 100ms 的间隙里，对象必须一直存活！

### 情况 B: 直接管理对象（只在同步代码中可行）

```cpp
// ✅ 这样可以
void processConnection(int connfd) {
    TcpStream conn(connfd);
    
    // 同步处理，函数返回前完成所有操作
    while (true) {
        // 读、处理、写
    }
    
    // conn 在这里立即销毁，没有悬空引用
}
```

**关键：没有任何其他地方持有 conn 的引用！**

---

## 三种内存管理方式的对比

### 1. 栈分配（Stack Allocation）

```cpp
TcpStream conn(fd);  // 在栈上

// ❌ 危险：如果 lambda 异步调用，conn 已销毁
channel->setReadCallback([&conn]() {
    conn.recvRaw(...);  // 可能段错误
});
```

| 优点 | 缺点 |
|------|------|
| 简单 | 对象生命周期固定 |
| 效率高 | 无法满足异步需求 |
| - | 如果需要延长生命周期，无法做到 |

### 2. 堆分配 + 手动管理（Raw Pointer）

```cpp
TcpStream* conn = new TcpStream(fd);

channel->setReadCallback([conn]() {
    conn->recvRaw(...);
});

// ❌ 问题：谁负责 delete？
// 如果在 handleConnection 中 delete，lambda 下次调用时段错误
// 如果不 delete，内存泄漏
// 如果在 Channel 析构时 delete，可能 lambda 还在使用
```

| 优点 | 缺点 |
|------|------|
| 手动控制生命周期 | ❌ 容易泄漏 |
| - | ❌ 容易野指针 |
| - | ❌ 容易重复 delete |

### 3. 智能指针 - shared_ptr（我们的选择）

```cpp
auto conn = std::make_shared<TcpStream>(fd);

channel->setReadCallback([conn]() {  // 自动增加引用计数
    conn->recvRaw(...);  // ✅ 安全！
});

// ✅ 自动管理：
// - 最后一个引用释放时自动 delete
// - 无需手动管理
// - 异步安全
```

| 优点 | 缺点 |
|------|------|
| ✅ 自动管理 | 轻微性能开销 |
| ✅ 无泄漏 | 引用计数管理 |
| ✅ 异步安全 | - |
| ✅ 值捕获安全 | - |

---

## 你的代码中的 shared_ptr 用途

### 1. Connection 对象

```cpp
auto conn = std::make_shared<TcpStream>(...);

channel->setReadCallback([conn]() {  // ← conn 被 lambda 值捕获
    handleConnection(loop, *conn);
});
```

**为什么需要：**
- lambda 异步执行（稍后被 epoll 触发）
- conn 需要存活到 lambda 被调用
- 使用 shared_ptr 让 lambda 捕获时自动管理生命周期

### 2. Channel 对象

```cpp
auto connChannel = std::make_shared<Channel>(...);

connChannel->setReadCallback([conn, &loop]() {
    handleConnection(loop, *conn);
});

loop.addChannel(std::move(connChannel));
```

**为什么需要：**
- channel 被移到 EventLoop
- EventLoop 需要长期持有 channel
- shared_ptr 让 EventLoop 安全地管理 channel 的生命周期

### 3. Lambda 内的关系图

```
创建：
  conn ──┐
         ├─→ lambda 中被值捕获
         ↓
  channel ──→ EventLoop（通过 addChannel）
         └─→ lambda 中的回调
```

**生命周期关系：**
```
conn 的引用者：
  ├─ main 作用域（create_shared）  ref = 1
  ├─ lambda 值捕获              ref = 2
  └─ handleConnection 参数 ← 引用

channel 的引用者：
  ├─ main 作用域（create_shared） ref = 1
  ├─ setReadCallback 中的 lambda
  └─ loop.addChannel()            ref = 2
```

---

## 如果不用 shared_ptr 会怎样？

### 错误代码示例

```cpp
// ❌ 使用栈分配
void handleNewConnection(EventLoop& loop, ServerSocket& listenSock) {
    TcpStream conn(listenSock.acceptConnection());     // 栈上
    Channel channel(conn.getFd());                     // 栈上
    
    channel.setReadCallback([&conn, &loop]() {         // 引用捕获
        handleConnection(loop, conn);
    });
    
    loop.addChannel(std::move(channel));
}
// ← conn 和 channel 在这里销毁！

// 后来，epoll 触发事件
// lambda 被调用
// 访问 &conn → 野指针！❌
```

**可能的后果：**
1. **段错误（Segmentation Fault）**
   ```
   Segmentation fault (core dumped)
   ```

2. **内存损坏**
   - conn 的内存被其他对象重用
   - 读取错误的数据
   - 修改不该修改的东西

3. **随机崩溃**
   - 有时工作，有时崩溃
   - 很难调试！

---

## 何时使用何种方式

| 情况 | 建议 | 理由 |
|------|------|------|
| 对象在栈上使用后立即销毁 | 栈分配 | 简单、高效 |
| 对象需要被多个地方共享 | shared_ptr | 自动管理 |
| 对象在异步回调中使用 | shared_ptr | 保证生命周期 |
| 需要精确控制生命周期 | unique_ptr | 单一所有权 |
| 临时指针，不获取所有权 | 裸指针 | 轻量级 |

---

## 性能考量

### shared_ptr 的开销

```cpp
auto ptr = std::make_shared<T>();  // 分配控制块
ptr->method();                      // 引用计数检查（几纳秒）
auto copy = ptr;                    // 原子操作递增计数
ptr = nullptr;                      // 原子操作递减计数，可能析构
```

**实际开销：**
- 每次复制：1-2 纳秒（原子操作）
- 每次赋值：1-2 纳秒（原子操作）
- 内存开销：24 字节（控制块）

**对比：**
- 网络 I/O：毫秒级（1,000,000+ 纳秒）
- shared_ptr 开销：微不足道！

---

## 总结

### 为什么这个项目用 shared_ptr？

```
异步事件驱动架构
    ↓
对象生命周期 ≠ 函数作用域
    ↓
需要自动管理内存
    ↓
shared_ptr 是最佳选择
```

### 三个关键点

1. **异步性**：lambda 稍后被调用，对象必须存活
2. **共享性**：多个地方持有对象（main + lambda + EventLoop）
3. **安全性**：自动管理，无需手动 delete

### 代码模式

```cpp
// ✅ 标准模式
auto conn = std::make_shared<T>(...);           // 创建
channel->setReadCallback([conn]() { ... });     // 值捕获
loop.addChannel(std::move(channel));            // 移入

// ✅ 原因
// - conn 会在最后一个使用者释放它时自动销毁
// - lambda 捕获时引用计数增加
// - 完全异步安全
```

---

## 进阶：如何验证这一点？

创建一个测试来看看会发生什么：

```cpp
// 编译运行测试
#include <memory>
#include <thread>
#include <chrono>

std::function<void()> lambda;

void unsafe_version() {
    struct Data { int value = 42; };
    Data data;
    
    // 错误：引用捕获本地对象
    lambda = [&data]() {
        std::cout << data.value << std::endl;  // 危险！
    };
}

void safe_version() {
    struct Data { int value = 42; };
    auto data = std::make_shared<Data>();
    
    // 安全：值捕获 shared_ptr
    lambda = [data]() {
        std::cout << data->value << std::endl;  // 安全！
    };
}

int main() {
    unsafe_version();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    lambda();  // 可能段错误
    
    safe_version();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    lambda();  // 安全执行
}
```

---

**结论：在异步、事件驱动的服务器中，shared_ptr 不是奢侈品，而是必需品。**
