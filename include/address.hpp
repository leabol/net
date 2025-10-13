#pragma once

#include <stdexcept>
#include <netdb.h>
#include <string>
#include <utility> 

class socketAddr {
public:
    socketAddr(std::string hostname, std::string port)
        : hostname_(std::move(hostname)), port_(std::move(port)) {
        hints_.ai_family = AF_UNSPEC;
        hints_.ai_socktype = SOCK_STREAM;
    }

    ~socketAddr() {
        if (res_)
            freeaddrinfo(res_);
    }

    socketAddr(const socketAddr& other) = delete;
    socketAddr& operator=(const socketAddr& other) = delete;

    socketAddr(socketAddr&& other) noexcept 
        :hostname_(std::move(other.hostname_)),
        port_(std::move(other.port_)),
        hints_(other.hints_), res_(other.res_){
            other.res_ = nullptr;
    }

    socketAddr& operator=(socketAddr&& other) noexcept {
        if (this != &other) {
            hostname_ = std::move(other.hostname_);
            port_ = std::move(other.port_);
            hints_ = other.hints_;
            res_ = other.res_;

            other.res_ = nullptr;
        }
        return *this;
    }
    void Getaddrinfo(){
        int ret = getaddrinfo(hostname_.c_str(), port_.c_str(), &hints_, &res_);
        if (ret != 0) {
            throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(ret)));
        }
    }

    void set_server() {
        hints_.ai_flags = AI_PASSIVE;
    }
    std::pair<sockaddr*, socklen_t> get_addr() {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return {res_->ai_addr, res_->ai_addrlen};
    }
    std::pair<sockaddr*, socklen_t> get_addr() const {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return {res_->ai_addr, res_->ai_addrlen};
    }

    struct addrinfo* get_addr_list() {
        if (!res_) {
            throw std::runtime_error("get_addrinfo() must be called first");
        }
        return res_;
    }
private:
    std::string hostname_;
    std::string port_;
    struct addrinfo hints_{};
    struct addrinfo *res_{nullptr};
};