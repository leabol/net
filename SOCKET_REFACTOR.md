# Socket 头文件重构总结

## 重构前的问题

原来的 `socket.hpp` 中混合了多个职责：
- 基础 Socket 类（fd 管理、非阻塞设置）
- 服务端 TcpSocket（bind、listen、accept）
- 客户端 TcpConnection（send、recv）
- 所有实现都是内联的，全在头文件中
- 职责不清晰，难以维护

## 重构方案

### 1. 文件分离

#### `include/Socket.hpp` - 基类
- 管理 socket fd 生命周期
- 非阻塞模式设置
- `connectTo()` 方法（支持客户端连接）
- 实现在 `src/Socket.cpp`

#### `include/TcpSocket.hpp` - 服务端
- 服务端专用操作
- `bindTo()` - 绑定地址
- `startListening()` - 监听
- `acceptConnection()` - 接受连接
- 实现在 `src/TcpSocket.cpp`

#### `include/TcpConnection.hpp` - 连接
- 已建立连接的操作
- `sendAll()` - 发送数据
- `recvString()` - 接收数据
- `recvRaw()` - 原始接收
- 实现在 `src/TcpConnection.cpp`

### 2. 依赖关系
```
Socket（基类）
  ├─ TcpSocket（服务端）
  │  └─ 返回 TcpConnection
  └─ connectTo()（客户端）
     └─ 返回 TcpConnection

TcpConnection（连接，继承自 Socket）
  └─ 实现收发逻辑
```

### 3. 使用示例

**服务端：**
```cpp
#include "TcpSocket.hpp"
TcpSocket server;
server.bindTo(addr);
server.startListening();
TcpConnection conn = server.acceptConnection();
```

**客户端：**
```cpp
#include "Socket.hpp"
Socket client;
TcpConnection conn = client.connectTo(addr);
```

## 构建更新

`CMakeLists.txt` 现在包含：
```cmake
add_executable(server
    src/server.cpp
    src/Socket.cpp
    src/TcpSocket.cpp
    src/TcpConnection.cpp
    ...
)
```

## 优点

✅ 职责分离：基类、服务端、连接分开  
✅ 头文件精简：逻辑在 .cpp 中，加快编译  
✅ 易于维护：改动只需修改对应文件  
✅ 清晰的 API：类名直观反映功能  
✅ 向前兼容：改进处理 `EINPROGRESS` 非阻塞连接
