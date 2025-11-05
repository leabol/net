# 开发检查清单 - 避免资源管理错误

## 目录
1. [设计阶段](#设计阶段)
2. [实现阶段](#实现阶段)
3. [测试阶段](#测试阶段)
4. [代码审查](#代码审查)

---

## 设计阶段

### 对象生命周期设计

- [ ] 列出项目中的所有长生命周期对象
  ```cpp
  - TcpStream   ✓
  - Channel     ✓
  - EventLoop   ✓
  ```

- [ ] 对每个对象，明确以下问题：
  - [ ] 谁创建它？
  - [ ] 谁持有它的引用？
  - [ ] 何时销毁它？
  - [ ] 销毁时是否有其他地方还在用它？

### shared_ptr vs unique_ptr vs 栈分配

对每个对象，问自己三个问题：

- [ ] **对象需要被异步访问吗？**
  - 是 → shared_ptr
  - 否 → 继续

- [ ] **对象需要被多个地方引用吗？**
  - 是 → shared_ptr
  - 否 → 继续

- [ ] **对象的销毁时机是由调用者确定的吗？**
  - 是 → unique_ptr / 栈分配
  - 否 → shared_ptr

### 文档规范

- [ ] 为每个类写生命周期文档

```cpp
/**
 * TcpStream - TCP 连接流
 * 
 * 生命周期管理：
 *   由 shared_ptr 管理，支持多个引用
 *   
 * 创建：
 *   auto conn = std::make_shared<TcpStream>(fd);
 *   
 * 销毁：
 *   - 自动：当所有引用释放时
 *   - 手动：conn.release() 后调用 close(fd)
 *   
 * 注意：
 *   - 不能在析构后访问
 *   - lambda 值捕获时会增加引用计数
 *   
 * 完整文档：SMART_POINTER_GUIDE.md
 */
```

---

## 实现阶段

### Lambda 捕获检查

对每个 lambda，检查：

```cpp
// ❓ 这个 lambda 做什么？
channel->setReadCallback([conn, &loop]() {
    handleConnection(loop, *conn);
});

// ✅ 检查清单
- [ ] conn 是否值捕获？（是 ✓）
- [ ] loop 是否引用捕获？（是 ✓）
- [ ] conn 的生命周期是否足够长？（是 ✓）
- [ ] loop 的生命周期是否足够长？（否需要重新考虑）
```

**捕获规则：**
- [ ] 长生命周期对象 → 值捕获
- [ ] 短生命周期对象 → 引用捕获（小心！）
- [ ] 栈上的局部变量 → 不能引用捕获！

### 创建对象时

```cpp
// ❌ 错误
TcpStream conn(fd);  // 栈上

// ❌ 错误
TcpStream* conn = new TcpStream(fd);  // 忘记 delete

// ✅ 正确
auto conn = std::make_shared<TcpStream>(fd);  // shared_ptr 管理
```

- [ ] 都用 `std::make_shared` 吗？
- [ ] 没有手动 `new` 吗？
- [ ] 没有在栈上创建需要异步访问的对象吗？

### 添加到 EventLoop 时

```cpp
// ❌ 错误
loop.addChannel(channel);  // 引用转移，但所有权不清楚

// ✅ 正确
loop.addChannel(std::move(channel));  // 明确所有权转移
```

- [ ] 使用了 `std::move` 吗？
- [ ] 文档中说明了 EventLoop 现在持有这个对象吗？

### 关闭对象时（最重要！）

**标准三步骤（顺序不能错）：**

```cpp
void closeConnection(EventLoop &loop, TcpStream& conn) {
    int fd = conn.getFd();
    
    // 步骤 1️⃣：转移 fd 所有权
    conn.release();
    
    // 步骤 2️⃣：从 EventLoop 移除
    loop.removeChannel(fd);
    
    // 步骤 3️⃣：关闭 fd
    if (close(fd) == -1) {
        std::cerr << "close failed: " << strerror(errno) << std::endl;
    }
}
```

- [ ] 第一步是 `conn.release()`？
- [ ] 第二步是 `loop.removeChannel(fd)`？
- [ ] 第三步是 `close(fd)` 并检查返回值？
- [ ] 顺序没有改变吗？

### 错误处理

对每个可能失败的操作：

```cpp
// ❌ 不检查
close(fd);

// ✅ 检查
if (close(fd) == -1) {
    std::cerr << "close failed: " << strerror(errno) << std::endl;
}
```

- [ ] 所有 `close()` 都检查返回值吗？
- [ ] 所有 `removeChannel()` 都检查返回值吗？
- [ ] 所有错误都有适当的日志吗？

---

## 测试阶段

### Address Sanitizer 测试

```bash
# 1. 编译时启用 ASan
g++ -fsanitize=address -g -std=c++17 src/*.cpp -o server

# 2. 运行
./server

# 3. 检查是否有报错
# - use-after-free
# - double-free
# - memory-leak
```

- [ ] 代码用 AddressSanitizer 编译吗？
- [ ] 用 ASan 运行过测试吗？
- [ ] 没有报错吗？

### Valgrind 测试

```bash
# 1. 运行
valgrind --leak-check=full --show-leak-kinds=all ./server

# 2. 检查输出
# - "no leaks possible"
# - "definitely lost: 0 bytes"
```

- [ ] 用 Valgrind 运行过吗？
- [ ] 没有内存泄漏吗？
- [ ] 没有 double-free 吗？

### 功能测试

```bash
# 测试基本功能
echo "hello" | nc localhost 8090

# 测试多个连接
for i in {1..10}; do echo "hello $i" | nc localhost 8090; done

# 测试快速断开重连
for i in {1..100}; do 
  (echo "test"; sleep 0.1) | nc localhost 8090
done
```

- [ ] 基本回显功能工作吗？
- [ ] 多连接下正常吗？
- [ ] 快速断开重连正常吗？
- [ ] 有没有"Bad file descriptor"错误？
- [ ] 有没有内存泄漏增长？

### 压力测试

```bash
# 并发连接测试
ab -n 1000 -c 50 http://localhost:8090/

# 或使用 nc 并发
(for i in {1..100}; do echo "test"; sleep 0.01; done) | nc localhost 8090
```

- [ ] 高并发下稳定吗？
- [ ] 内存使用持续增长吗？
- [ ] 有没有文件描述符泄漏？

### 文件描述符检查

```bash
# 获取服务器 PID
ps aux | grep server

# 查看打开的 fd
lsof -p <PID> | wc -l

# 应该与活跃连接数一致
```

- [ ] fd 数量与连接数匹配吗？
- [ ] 连接关闭后 fd 数量减少吗？
- [ ] 有没有 fd 泄漏？

---

## 代码审查

### 自我审查

**每次修改后，问自己这些问题：**

#### 所有权问题

- [ ] 每个 `new` 都对应 `delete` 吗？
- [ ] 每个 `make_shared` 都会最终被销毁吗？
- [ ] 有没有未初始化的指针？

#### 生命周期问题

- [ ] 每个引用都指向活着的对象吗？
- [ ] 有没有悬空指针？
- [ ] 有没有用完即弃的 `make_shared`？

```cpp
// ❌ 错误：用完即弃
loop.addChannel(std::make_shared<Channel>(...));
// make_shared 返回的 shared_ptr 在语句末尾销毁
// 立即执行了 removeChannel！

// ✅ 正确：保存引用
auto ch = std::make_shared<Channel>(...);
loop.addChannel(std::move(ch));
```

#### 关闭路径问题

- [ ] 所有 `close()` 都在 `removeChannel()` 之后吗？
- [ ] 所有 `close()` 都在 `release()` 之后吗？
- [ ] 有没有跳过任何一步的路径？

```cpp
// 检查所有这些地方是否都遵循三步骤：
- handleConnection 中关闭
- 读错误时关闭
- 写错误时关闭
- 连接 EOF 时关闭
- 连接超时时关闭（如有）
```

#### 错误处理问题

- [ ] 所有可能失败的操作都检查了吗？
- [ ] 错误路径是否也会正确关闭资源？
- [ ] 有没有在错误时忘记清理？

### 代码审查清单

共同审查代码时：

**对于每个类：**
- [ ] 析构函数是否正确释放资源？
- [ ] 是否有资源泄漏的可能？
- [ ] shared_ptr 的使用是否正确？

**对于每个函数：**
- [ ] 参数是否明确所有权？
- [ ] 返回值是否明确所有权？
- [ ] 有没有悬空指针的可能？

**对于每个 Lambda：**
- [ ] 捕获列表是否正确？
- [ ] 值捕获 vs 引用捕获选择是否正确？
- [ ] 生命周期是否足够长？

**对于每个 close/remove：**
- [ ] 是否按正确顺序？
- [ ] 是否检查了返回值？
- [ ] 有没有重复关闭的可能？

---

## 常见错误检查

使用此清单快速检测常见错误：

```bash
# 1. 查找所有 close() 调用
grep -n "close(" src/*.cpp

# 确保每个都在正确的位置

# 2. 查找所有 make_shared
grep -n "make_shared" src/*.cpp

# 确保都被保存或立即使用

# 3. 查找所有 release()
grep -n "release()" src/*.cpp

# 确保前后都有 removeChannel 和 close

# 4. 查找所有 removeChannel()
grep -n "removeChannel" src/*.cpp

# 确保都在 release 后、close 前

# 5. 查找所有 lambda
grep -n "\[" src/*.cpp

# 检查每个 lambda 的捕获列表是否正确
```

---

## 快速参考

### 三步骤关闭（必须记住！）

```cpp
int fd = object.getFd();
object.release();              // 步骤 1
loop.removeChannel(fd);        // 步骤 2
close(fd);                     // 步骤 3
```

### Lambda 捕获规则

```cpp
[shared_ptr_var]              // ✅ 值捕获长生命周期
[&short_lived_ref]            // ⚠️ 引用捕获短生命周期（危险）
[&loop, conn]                 // ✅ 混合（loop 短，conn 长）
```

### 何时用 shared_ptr

```
异步 + 共享 + 不可控
    ↓
shared_ptr
```

### 测试命令速查

```bash
# 编译 + ASan
g++ -fsanitize=address -g -std=c++17 src/*.cpp -o server

# Valgrind
valgrind --leak-check=full ./server

# 文件描述符检查
lsof -p $(pgrep server) | wc -l

# 连接测试
echo "test" | nc localhost 8090
```

---

## 更新历史

- **v1.0** (Oct 31): 初始版本
  - 包含设计、实现、测试三个阶段
  - 添加了常见错误快速检查

---

**记住：好的设计需要遵循清单。好的代码需要多次审查。安全的系统需要工具验证。**
