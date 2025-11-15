# 文档快速索引 🔍

## 按场景查找

### 🆕 我是初学者
```
1. 打开 README_DOCS.md
2. 找到"我是新手，想学习网络编程"部分
3. 按推荐顺序阅读
4. 预计时间：2-3 小时
```

### 🐛 我在调试程序，代码有问题
```
1. 打开 QUICK_REFERENCE.md
2. 在"常见问题"中查找相关错误
3. 如果找到了，按说明操作
4. 如果没找到，打开 LEARNING_NOTES.md → "常见错误"
5. 预计时间：5-30 分钟
```

### 🎓 我想深度学习某个概念
```
1. 打开 README_DOCS.md → "关键概念速查"
2. 找到对应概念的文档位置
3. 打开该文档的相应章节
4. 预计时间：30 分钟 - 2 小时
```

### 🔧 我想参考代码
```
1. 打开 QUICK_REFERENCE.md → "代码模板"
2. 或打开 LEARNING_NOTES.md → "最佳实践"
3. 复制适用的代码模板
4. 预计时间：5 分钟
```

### 📖 我想看完整案例
```
1. 打开 DEBUG_REPORT.md
2. 从"问题现象"开始读
3. 看完整的调试过程
4. 了解解决方案
5. 预计时间：30 分钟
```

---

## 按错误类型查找

| 错误类型 | 症状 | 快速查找 |
|---------|------|---------|
| **CPU 100%** | 风扇狂转 | QUICK_REFERENCE.md → CPU 占用 100% |
| **数据丢失** | 收不全 | QUICK_REFERENCE.md → ET 模式漏了数据？ |
| **栈溢出** | 程序崩溃 | LEARNING_NOTES.md → 错误 3 |
| **fd 混乱** | 关错 fd | LEARNING_NOTES.md → 错误 5 |
| **Bad fd** | close 失败 | DEBUG_REPORT.md 或 QUICK_REFERENCE.md → close 报错 |
| **连接卡** | 无响应 | QUICK_REFERENCE.md → 连接卡住不动？ |

---

## 按文件查找

### src/server.cpp
- handleConnection 问题 → LEARNING_NOTES.md 错误 2（ET 模式）
- handleNewConnection 问题 → DEBUG_REPORT.md（shared_ptr）
- closeConnection 问题 → LEARNING_NOTES.md 错误 6

### src/SocketBase.cpp
- 构造/析构问题 → LEARNING_NOTES.md 深入理解 1（Socket 生命周期）
- RAII 问题 → LEARNING_NOTES.md 设计模式 1

### src/TcpStream.cpp
- recv 问题 → LEARNING_NOTES.md 错误 3（EINTR）、错误 1（EAGAIN）
- send 问题 → LEARNING_NOTES.md 错误 4（返回值检查）

### src/EventLoop.cpp
- removeChannel 问题 → DEBUG_REPORT.md（shared_ptr 生命周期）

### src/EpollPoller.cpp
- epoll 问题 → LEARNING_NOTES.md 深入理解 3（epoll 工作流）

---

## 按概念查找

### 阻塞 vs 非阻塞
→ LEARNING_NOTES.md → 核心概念 1

### LT vs ET 模式
→ LEARNING_NOTES.md → 核心概念 2

### RAII 模式
→ LEARNING_NOTES.md → 设计模式 1

### 回调模式
→ LEARNING_NOTES.md → 设计模式 2

### 异常处理
→ LEARNING_NOTES.md → 设计模式 3

### Socket 生命周期
→ LEARNING_NOTES.md → 深入理解 1

### TCP 三次握手
→ LEARNING_NOTES.md → 深入理解 2

### epoll 工作流
→ LEARNING_NOTES.md → 深入理解 3

### shared_ptr 生命周期
→ DEBUG_REPORT.md → "原因分析" 或 LEARNING_NOTES.md → 错误 6

### EAGAIN 正确处理
→ LEARNING_NOTES.md → 错误 1 & 错误 7
→ QUICK_REFERENCE.md → "收到一条消息后连接就断了？"

---

## 快速命令

```bash
# 查看所有文档
ls -lh /home/lea/os/net/*.md

# 统计文档行数
wc -l /home/lea/os/net/*.md

# 搜索特定关键词
grep -n "关键词" /home/lea/os/net/LEARNING_NOTES.md

# 查看某个错误
grep -A 30 "错误 6" /home/lea/os/net/LEARNING_NOTES.md
grep -A 30 "错误 7" /home/lea/os/net/LEARNING_NOTES.md

# 快速查看调试报告
cat /home/lea/os/net/DEBUG_REPORT.md | less
```

---

## 文档关键词

### LEARNING_NOTES.md 关键词
- 阻塞、非阻塞
- LT、ET
- EINTR、EAGAIN、EPIPE
- RAII、回调、异常
- 生命周期、握手、epoll

### QUICK_REFERENCE.md 关键词
- 快速查找
- 错误码
- 检查清单
- 常见问题
- 代码模板

### DEBUG_REPORT.md 关键词
- 双重关闭
- shared_ptr
- lambda
- 引用计数
- 调试过程

### README_DOCS.md 关键词
- 文档导航
- 学习路线
- 快速查找指南
- 错误汇总
- 文档对应

---

## 一页纸速查

| 问题 | 解决方案 | 文档 |
|------|---------|------|
| 忙轮询 | return EAGAIN | QUICK_REFERENCE |
| 数据丢失 | while 循环读 | QUICK_REFERENCE |
| 栈溢出 | while EINTR | QUICK_REFERENCE |
| 数据漏发 | 检查 send 返回 | QUICK_REFERENCE |
| 错关 fd | accept 返回新 fd | QUICK_REFERENCE |
| Bad fd | release→remove→close | QUICK_REFERENCE |

---

## 推荐阅读路线

### 路线 A：快速上手（1 小时）
1. README_DOCS.md（10 分钟）- 了解全貌
2. QUICK_REFERENCE.md（20 分钟）- 快速查找
3. DEBUG_REPORT.md（30 分钟）- 案例学习

### 路线 B：深度学习（3 小时）
1. README_DOCS.md（10 分钟）- 导航
2. LEARNING_NOTES.md（2 小时）- 系统学习
3. DEBUG_REPORT.md（30 分钟）- 实战案例
4. QUICK_REFERENCE.md（10 分钟）- 工具查找

### 路线 C：问题解决（按需）
1. 遇到问题
2. QUICK_REFERENCE.md（快速查找）
3. 找到类似问题？深入 LEARNING_NOTES.md
4. 还是没解决？看 DEBUG_REPORT.md 的调试思路

---

**最后更新：2025-10-28**
