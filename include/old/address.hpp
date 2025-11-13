#pragma once

#include <stdexcept>
#include <netdb.h>
#include <string>
#include <utility> 

class SocketAddr {
public:
    SocketAddr(std::string hostname, std::string port)
        : hostname_(std::move(hostname)), port_(std::move(port)) {
        hints_.ai_family = AF_UNSPEC;
        hints_.ai_socktype = SOCK_STREAM;
        resolve();
    }

    static SocketAddr for_server(std::string port) {
        SocketAddr addr;
        addr.hostname_ = "";
        addr.port_ = std::move(port);
        addr.hints_.ai_family = AF_UNSPEC;
        addr.hints_.ai_socktype = SOCK_STREAM;
        addr.hints_.ai_flags = AI_PASSIVE;
        addr.resolve();
        return addr;
    }

    ~SocketAddr() {
        if (res_)
            freeaddrinfo(res_);
    }

    SocketAddr(const SocketAddr& other) = delete;
    SocketAddr& operator=(const SocketAddr& other) = delete;

    SocketAddr(SocketAddr&& other) noexcept 
        :hostname_(std::move(other.hostname_)),
        port_(std::move(other.port_)),
        hints_(other.hints_), res_(other.res_){
            other.res_ = nullptr;
    }
    SocketAddr& operator=(SocketAddr&& other) noexcept {
        if (this != &other) {
            hostname_ = std::move(other.hostname_);
            port_ = std::move(other.port_);
            hints_ = other.hints_;
            res_ = other.res_;
            other.res_ = nullptr;
        }
        return *this;
    }

    //resolve the hostname
    void resolve(){
        const char* node = hostname_.empty() ? nullptr : hostname_.c_str();
        int ret = getaddrinfo(node, port_.c_str(), &hints_, &res_);
        if (ret != 0) {
            throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(ret)));
        }
    }

    std::pair<sockaddr*, socklen_t> endpoint() {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return {res_->ai_addr, res_->ai_addrlen};
    }
    std::pair<sockaddr*, socklen_t> endpoint() const {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return {res_->ai_addr, res_->ai_addrlen};
    }

    struct addrinfo* getAddrinfoList() {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return res_;
    }
private:
    SocketAddr() = default;
private:
    std::string hostname_;
    std::string port_;
    struct addrinfo hints_{};
    struct addrinfo *res_{nullptr};
};