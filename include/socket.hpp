#include <stdexcept>    
#include <string>       
#include <cstring>      
#include <cerrno>       
#include <netinet/in.h> 
#include <unistd.h>

class tcpConnection;

class tcpSocket {
public:
    tcpSocket() {
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == -1) {
            throw std::runtime_error("socket creation failed: "  + std::string(strerror(errno)));
        }
        int yes = 1;
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }
    
    ~tcpSocket() {
        if (socket_ != -1)
            close(socket_);
    }

    void Bind(const sockaddr *addr, socklen_t len) {
        if (bind(socket_, addr, len) == -1) {
            throw std::runtime_error("bind failed"+ std::string(strerror(errno)));
        }
    }

    void  Listen(int n = 10) {
        if (listen(socket_, n) == -1) {
            throw std::runtime_error("listen failed"+ std::string(strerror(errno)));
        }
    }

    void Connect(const sockaddr *addr, socklen_t len) {
        if (connect(socket_, addr, len) == -1) {
            throw std::runtime_error("connect is failed"+ std::string(strerror(errno)));
        }
    }

    tcpConnection Accept(sockaddr *addr = nullptr, socklen_t *len = nullptr);

    int get_fd() const {
        return socket_;
    }

    tcpSocket(const tcpSocket&) = delete;
    tcpSocket& operator=(const tcpSocket&) = delete;

    tcpSocket(tcpSocket&& other) noexcept : socket_(other.socket_) {
        other.socket_ = -1;
    };
    tcpSocket& operator=(tcpSocket&& other) noexcept {
        if (this != &other) {
            if (socket_ != -1)
                close(socket_);
            socket_ = other.socket_;
            other.socket_ = -1;
        }
        return *this;
    }
private:
    int socket_{-1};
};

class tcpConnection {
public:
    explicit tcpConnection(int fd) : socket_(fd) {}
    ~tcpConnection() { 
        if (socket_ != -1) 
            close(socket_); 
    }

    int get_fd() const { 
        return socket_; 
    }
    
    tcpConnection(const tcpConnection&) = delete;
    tcpConnection& operator=(const tcpConnection&) = delete;

    tcpConnection(tcpConnection&& other) noexcept : socket_(other.socket_) {
        other.socket_ = -1;
    }
    tcpConnection& operator=(tcpConnection&& other) noexcept {
        if (this != &other) {
            if (socket_ != -1) close(socket_);
            socket_ = other.socket_;
            other.socket_ = -1;
        }
        return *this;
    }

private:
    int socket_{-1};
};

tcpConnection tcpSocket::Accept(sockaddr *addr, socklen_t *len ){
    int client = accept(socket_, addr, len);
    if (client == -1) {
        throw std::runtime_error("accept faild"+ std::string(strerror(errno)));
    }
    return tcpConnection(client);
}
