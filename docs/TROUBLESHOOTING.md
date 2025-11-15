# 故障排查（症状 → 原因 → 诊断 → 解决 | 附代码）

本节聚焦当前实现（不含 src/old），结合常见现象给出实操级诊断步骤与修复代码片段。

## epoll_ctl(ADD) 报 EEXIST
- 原因：
	- 用户态/内核态不同步（历史路径漏 DEL、异常返回未处理、先前版本残留）
	- 重复注册（多次 enable、并发/重入）
	- fd 复用/跨 exec 继承（CLOEXEC 缓解）
- 诊断：
	- 观察日志是否只在 `ADD` 后才写入 `channels_[fd]`；确认 `Channel::added_` 是否在成功后才置 true
	- 确认 `updateChannel` 是否单线程串行调用（只在 EventLoop 线程）
- 解决：采用“MOD 优先 + 成功后再更新缓存”的容错策略（已在仓库实现），代码骨架：
	```cpp
	// 伪码：优先 MOD，不存在回退 ADD；仅在成功后更新 map 与 added_
	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
			if (errno == ENOENT || !ch->isAdded()) {
					if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
							if (errno == EEXIST) {
									(void)epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
							} else { /* log error */ }
					} else {
							ch->setAdded(true); channels_[fd] = ch;
					}
			} else { /* log error */ }
	} else {
			ch->setAdded(true); if (!inMap) channels_[fd] = ch;
	}
	```

## epoll_ctl(MOD) 报 ENOENT/EINVAL
- 原因：条目未注册、fd 已关闭/无效、data/事件集非法
- 诊断：
	- 检查关闭顺序：先从 epoll DEL，再 close fd；重复 close 也会触发 EINVAL
	- 确认传入 `event.data.ptr` 指向有效 Channel
- 解决：回退 ADD；清理状态位与 map。代码片段：
	```cpp
	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
			if (errno == ENOENT) {
					if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
							/* log */
					} else { ch->setAdded(true); channels_[fd] = ch; }
			} else { /* log */ }
	}
	```

## accept 抽风/卡死/抛异常
- 原因：监听 fd 非阻塞设置缺失；上层用异常表示“暂无连接”；循环未读到 EAGAIN 就退出
- 诊断：
	- 确认监听 fd 是非阻塞（`accept4` 带 `SOCK_NONBLOCK` 或 fcntl 设置）
	- 上层是否在 `EAGAIN/EINTR` 场景抛异常而中断循环
- 解决：非阻塞 + 循环到 EAGAIN，失败返回 -1 由上层判断。正确范式：
	```cpp
	for (;;) {
			InetAddress peer;
			int cfd = acceptSocket_.accept(peer); // 失败返回 -1，不抛异常
			if (cfd >= 0) {
					if (newConnCb) newConnCb(cfd, peer); else ::close(cfd);
					continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 本轮无更多
			if (errno == EINTR) continue;        // 重试
			if (errno == ECONNABORTED) continue; // 忽略继续
			// 其他错误：记录并 break
			LOG_ERROR("accept failed: {}", strerror(errno));
			break;
	}
	```

## 写入 EPIPE / 收到 SIGPIPE
- 原因：对端已关闭；向关闭套接字写会产生 `SIGPIPE` 并返回 `EPIPE`
- 诊断：
	- 是否未忽略 `SIGPIPE`；写回调是否未处理 `EPIPE/ECONNRESET`
- 解决：
	- 启动时忽略 `SIGPIPE`：
		```c
		struct sigaction sa = {0};
		sa.sa_handler = SIG_IGN; sigaction(SIGPIPE, &sa, NULL);
		```
	- 或发送时使用 `MSG_NOSIGNAL`（示例）：
		```cpp
		ssize_t n = ::send(fd, buf, len, MSG_NOSIGNAL);
		```
	- 进入关闭流程：从 epoll 移除、关闭 fd、释放资源

## 连接建立慢/失败（DNS/ADDRCONFIG）
- 原因：`getaddrinfo` 阻塞；容器/无全局地址环境下 `AI_ADDRCONFIG` 可能返回空
- 诊断：
	- 观察 `getaddrinfo` 调用是否在请求路径；检查 `hints.ai_flags` 是否含 `AI_ADDRCONFIG`
- 解决：
	- 服务启动阶段做解析并缓存结果；必要时关闭 `AI_ADDRCONFIG`
	- 代码示例（关闭该标志）：
		```cpp
		// InetAddress 内支持修改 hints 的扩展实现示意：
		hints_.ai_flags &= ~AI_ADDRCONFIG; // 或提供构造参数控制
		```

## 读到 0 / 半关闭处理混乱
- 原因：未区分 `n==0`（对端关闭）与错误；未监听 `EPOLLRDHUP`
- 诊断：读回调是否在 `n==0` 时触发关闭逻辑；是否注册 `EPOLLRDHUP`
- 解决：
	```cpp
	for (;;) {
			ssize_t n = ::read(fd, buf, sizeof(buf));
			if (n > 0) { /* 处理数据 */ }
			else if (n == 0) { /* 对端关闭：移除epoll+close */ break; }
			else {
					if (errno == EINTR) continue;
					if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 本轮读完
					/* 其他错误：记录并关闭 */ break;
			}
	}
	```

## 非阻塞 connect 判定错误
- 原因：把 `EINPROGRESS` 当作失败；未使用 `SO_ERROR` 校验
- 诊断：在 `connect` 返回 `-1` 且 `errno==EINPROGRESS` 时是否注册了 `EPOLLOUT`
- 解决：
	```cpp
	::fcntl(fd, F_SETFL, O_NONBLOCK);
	int r = ::connect(fd, addr, len);
	if (r == -1 && errno == EINPROGRESS) {
			// 注册 EPOLLOUT；等可写后：
			int err=0; socklen_t elen=sizeof(err);
			::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
			if (err == 0) { /* 连接成功 */ }
			else { /* 失败：err为真实错误码 */ }
	}
	```

## 日志丢失/无颜色
- 原因：异步刷新周期过长；控制台不支持 ANSI；编译期裁剪级别
- 解决：
	- 提高 `flush_on` 等级或缩短 `flush_every`；确保使用 VS Code 集成终端/支持 ANSI 的终端
	- 调整 `SPDLOG_ACTIVE_LEVEL` 或设置 `SPDLOG_LEVEL=debug` 等环境变量
