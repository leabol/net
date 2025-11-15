# 代码设计与接口约定（不含 src/old）

## Socket
### 绑定遍历示例
```cpp
void Socket::bindAddr(const InetAddress& addr) const {
	const addrinfo* list = addr.getAddrinfoList();
	for (auto* rp=list; rp; rp=rp->ai_next) {
		if (::bind(fd(), rp->ai_addr, rp->ai_addrlen) == 0) return;
	}
	throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
}
```
- 语义：非阻塞路径返回 `-1` 并由上层判定 `errno`（避免把“可恢复分支”做成异常）。
- `connect`：若迁移到非阻塞，需要支持 `EINPROGRESS` → `EPOLLOUT + SO_ERROR` 判定。
- 绑定：遍历 `addrinfo` 直到成功；日志在失败时输出最后错误。

## InetAddress
### 从 sockaddr 构造与混合来源访问
```cpp
InetAddress peer{reinterpret_cast<sockaddr*>(&ss), len}; // 直接持有
auto* sa = peer.addr(); auto slen = peer.addrlen();      // 统一访问
```
- 所有权：不可拷贝、可移动；`resolve()` 前释放旧结果避免泄漏。
- 构造：支持从 `sockaddr` 直接构造，避免二次解析；`addr()/addrlen()` 优先返回 `sockaddr_storage`。
- 扩展（待选）：`toString()`、非抛异常接口、定制 `hints`。

## Channel
### 回调顺序推荐（避免数据丢失）
```cpp
void Channel::handleEvent() {
	const uint32_t rev = revents_;
	if (rev & EPOLLIN) { if (readCallback_) readCallback_(); }
	if (rev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
		if (closeCallback_) closeCallback_();
		return;
	}
	if (rev & EPOLLOUT) { if (writeCallback_) writeCallback_(); }
}
```
- 状态：新增 `added_` 标志，由 `EpollPoller` 维护，避免重复 ADD。
- 回调顺序：`HUP/ERR` 与 `IN` 并发时，考虑先尝试读一次再处理关闭，避免丢数据。
- 生命周期：创建方管理；关闭 fd 前先 `remove()` 从 epoll 脱钩。

## EpollPoller
### MOD 优先策略（仅成功后更新缓存与状态）
```cpp
int fd = ch->getFd(); epoll_event ev{.events=ch->getInterestedEvents(), .data{.ptr=ch}};
if (ev.events == 0) {
	if (ch->isAdded()) (void)epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
	ch->setAdded(false); channels_.erase(fd); return;
}
if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
	if (errno==ENOENT || !ch->isAdded() || channels_.find(fd)==channels_.end()) {
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
			if (errno==EEXIST) (void)epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
			else { /* log */ return; }
		}
		ch->setAdded(true); channels_[fd] = ch;
	} else { /* log */ }
} else {
	ch->setAdded(true); if (!channels_.count(fd)) channels_[fd] = ch;
}
```
- Map 与内核一致性：仅在 `epoll_ctl` 成功后更新 `channels_` 与 `added_`。
- 策略：MOD 优先；`ENOENT` 或未注册则 ADD；ADD 若 `EEXIST` 再 MOD。
- 禁用：`events==0` → DEL + 擦除映射 + `added_=false`；`DEL` 忽略 `ENOENT`。
- 错误记录：对 `epoll_ctl` ADD/MOD/DEL 失败均打日志，便于定位。

## Acceptor
### 接受循环（健壮版）
```cpp
for (;;) {
	InetAddress peer; int cfd = acceptSocket_.accept(peer);
	if (cfd >= 0) {
		if (cb) cb(cfd, peer); else ::close(cfd);
		continue;
	}
	if (errno==EAGAIN||errno==EWOULDBLOCK) break;
	if (errno==EINTR || errno==ECONNABORTED) continue;
	// 其他错误：记录并 break
	break;
}
```
- 监听 fd 推荐 LT；循环 `accept` 到 `EAGAIN`。
- 未设置新连接回调时，必须关闭新 fd 防止泄漏。
- 初始化顺序：准备 `Socket` → `Channel(fd)` → 设置读回调 → `listen()+enableReading()`。

## EventLoop
### 串行化更新（示意）
```cpp
// 若来自其他线程，投递到 loop 线程执行
void runInLoop(std::function<void()> fn) {
	if (inLoopThread()) fn(); else queueInLoop(std::move(fn));
}
```
- 单线程模型：保证 `update/remove` 在 loop 线程串行调用；跨线程通过任务队列转发。
- 退出条件：实际工程需要 `quit()` 标志或外部取消机制。
