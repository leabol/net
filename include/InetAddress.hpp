#pragma once

#include <netdb.h>
#include <sys/socket.h>

#include <stdexcept>
#include <string>
#include <utility>
// NOTE: 如需将地址转字符串，可能需要 <arpa/inet.h> 的 inet_ntop（可选）。

namespace Server {

class InetAddress {
  public:
    explicit InetAddress(std::string host, std::string port)
        : hostname_(std::move(host)), port_(std::move(port)) {
        // 设置返回addrinfo的限制条件
        hints_.ai_family = AF_INET;  // 指定具体返回的是 IPv4 类型的地址
        // TODO: 可改进 - 支持 IPv6/双栈：改为 AF_UNSPEC，并在使用端按需设置 IPV6_V6ONLY。
        hints_.ai_socktype = SOCK_STREAM;  // Specify the required socket type to return the address
                                           // and protocol information.
        hints_.ai_protocol = 0;
        hints_.ai_flags |= AI_ADDRCONFIG;  // 仅返回本机可达协议族地址
        // NOTE: 在容器或未配置全局地址的主机上，AI_ADDRCONFIG 可能导致返回集为空。
        // TODO: 可根据运行环境提供开关以禁用该标志。
        resolve();  // 开始解析并获取地址信息
    };
    // for server to listen port
    explicit InetAddress(std::string port) : port_(std::move(port)) {
        hints_.ai_flags =
            AI_PASSIVE | AI_ADDRCONFIG;  // 服务端：绑定所有本地地址；仅返回本机可达协议族
        // TODO: 可改进 - 纯数字端口时加入 AI_NUMERICSERV，跳过服务名解析以提升性能。
        hints_.ai_family   = AF_INET;
        hints_.ai_socktype = SOCK_STREAM;
        hints_.ai_protocol = 0;
        resolve();
    };
    ~InetAddress() noexcept {
        if (res_ != nullptr) {
            freeaddrinfo(res_);
        }
        // NOTE: 若未来希望支持拷贝语义，需要实现深拷贝以避免双重释放。
    }

    // 不可拷贝，避免 addrinfo 链表双重释放
    InetAddress(const InetAddress&)            = delete;
    InetAddress& operator=(const InetAddress&) = delete;
    // TODO: 可改进 - 使用自定义删除器的 unique_ptr 封装 addrinfo，简化资源管理并允许移动聚合。

    // 可移动：转移所有权
    InetAddress(InetAddress&& other) noexcept
        : hostname_(std::move(other.hostname_))
        , port_(std::move(other.port_))
        , hints_(other.hints_)
        , res_(other.res_) {
        other.res_ = nullptr;
    }
    InetAddress& operator=(InetAddress&& other) noexcept {
        if (this != &other) {
            if (res_ != nullptr) {
                freeaddrinfo(res_);
            }
            hostname_  = std::move(other.hostname_);
            port_      = std::move(other.port_);
            hints_     = other.hints_;
            res_       = other.res_;
            other.res_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] const struct addrinfo* getAddrinfoList() const {
        if (res_ == nullptr) {
            throw std::runtime_error("resolve() must be called first");
        }
        return res_;
    };
    // TODO: 可改进 - 暴露迭代接口（begin()/end()）或返回 std::vector<addrinfo>
    // 供调用方枚举候选地址。

    // 便捷：返回首个地址（常用于 connect/bind）
    [[nodiscard]] const struct sockaddr* addr() const {
        if (res_ == nullptr) {
            throw std::runtime_error("resolve() must be called first");
        }
        return res_->ai_addr;
    }
    [[nodiscard]] socklen_t addrlen() const {
        if (res_ == nullptr) {
            throw std::runtime_error("resolve() must be called first");
        }
        return static_cast<socklen_t>(res_->ai_addrlen);
    }
    // TODO: 可改进 - 提供 toString()（例如 "ip:port"），便于日志与调试；需要 inet_ntop。
    // TODO: 可改进 - 提供非抛异常的接口（返回错误码/expected），便于在库场景减少异常开销。
  private:
    void resolve() {
        if (res_ != nullptr) {  // 再解析前释放旧结果，避免泄漏
            freeaddrinfo(res_);
            res_ = nullptr;
        }
        const char* node = hostname_.empty() ? nullptr : hostname_.c_str();
        int         ret  = getaddrinfo(node, port_.c_str(), &hints_, &res_);
        if (ret != 0) {
            throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(ret)));
        }
        // TODO: 可改进 - getaddrinfo 为阻塞调用，必要时考虑超时/取消（例如使用 glibc 的
        // getaddrinfo_a 或外部超时机制）。
        // TODO: 可改进 - 考虑负缓存与重试策略（网络短暂错误时）。
    }

    std::string     hostname_;
    std::string     port_;
    struct addrinfo hints_ {};  // 地址解析限制条件（已零初始化）
    // TODO: 可改进 - 提供 setter 或构造参数允许调用方定制 hints（例如协议族/协议/flags）。
    struct addrinfo* res_{nullptr};  // 解析后的addrinfo链表, 一个主机名对应多个ip
    // TODO: 可改进 - 若端口是数字字符串，可在构造时校验范围 [1,65535] 并在需要时存储数值化端口。
};
}  // namespace Server
