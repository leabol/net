#include "http/HttpServer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "Log.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpStatus.hpp"
#include "http/HttpUtils.hpp"

namespace Http {

HttpServer::HttpServer(Server::EventLoop*         loop,
                       const Server::InetAddress& listenAddr,
                       std::filesystem::path      storageDir,
                       std::filesystem::path      staticDir)
    : server_(loop, listenAddr)
    , storageDir_(std::move(storageDir))
    , staticDir_(std::move(staticDir)) {
    storageDir_ = std::filesystem::absolute(storageDir_);
    staticDir_  = std::filesystem::absolute(staticDir_);

    std::error_code ec;
    std::filesystem::create_directories(storageDir_, ec);
    if (ec) {
        LOG_WARN("failed to ensure storage dir {}: {}", storageDir_.string(), ec.message());
    }

    server_.setConnectionCallback(
        [this](const Server::TcpServer::TcpConnectionPtr& conn) { this->onConnection(conn); });
    server_.setMessageCallback([this](const Server::TcpServer::TcpConnectionPtr& conn,
                                      std::string& data) { this->onMessage(conn, data); });
}

void HttpServer::start() {
    server_.start();
}

void HttpServer::onConnection(const Server::TcpServer::TcpConnectionPtr& conn) {
    const int fd = conn->fd();
    auto      it = contexts_.find(fd);
    if (it == contexts_.end()) {
        contexts_.emplace(fd, ConnectionContext{});
        LOG_INFO("http connection fd={} established", fd);
    } else {
        contexts_.erase(it);
        LOG_INFO("http connection fd={} removed", fd);
    }
}

void HttpServer::onMessage(const Server::TcpServer::TcpConnectionPtr& conn, std::string& data) {
    auto& ctx = contexts_[conn->fd()];
    ctx.buffer.append(data);  // 转载读取的字节流

    HttpRequest req;
    // 一次可能会收到多个请求,需要循环多次解析
    while (parseHttpRequest(ctx, req)) {
        handleRequest(conn, req);
        req = HttpRequest{};
    }
}
void HttpServer::handleRequest(const Server::TcpServer::TcpConnectionPtr& conn,
                               const HttpRequest&                         req) {
    if (req.method == "GET") {
        handleGet(conn, req);
    } else if (req.method == "POST") {
        handlePost(conn, req);
    } else if (req.method == "DELETE") {
        handleDelete(conn, req);
    } else {
        HttpResponse resp;
        resp.setStatus(StatusCode::kMethodNotAllowed, "Method Not Allowed");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Unsupported method\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
    }
}

void HttpServer::handleGet(const Server::TcpServer::TcpConnectionPtr& conn,
                           const HttpRequest&                         req) {
    const std::string cleanPath = stripQuery(req.path);
    if (cleanPath == "/" || cleanPath == "/index.html") {
        replyStaticFile(conn, "index.html");
        return;
    }

    // 获取文件列表
    constexpr std::string_view prefix = "/api/files";
    if (cleanPath == prefix) {
        replyFileList(conn);
        return;
    }
    // 获取具体文件
    if (cleanPath.rfind(prefix, 0) == 0 && cleanPath.size() > prefix.size() + 1) {
        const std::string name = cleanPath.substr(prefix.size() + 1);
        replyDownload(conn, name);
        return;
    }

    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("Resource not found\n");
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::handlePost(const Server::TcpServer::TcpConnectionPtr& conn,
                            const HttpRequest&                         req) {
    const std::string cleanPath = stripQuery(req.path);
    if (cleanPath == "/api/files") {
        handleUpload(conn, req);
        return;
    }
    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("POST target not found\n");
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::handleDelete(const Server::TcpServer::TcpConnectionPtr& conn,
                              const HttpRequest&                         req) {
    const std::string          cleanPath = stripQuery(req.path);
    constexpr std::string_view prefix    = "/api/files";
    if (cleanPath.rfind(prefix, 0) == 0 && cleanPath.size() > prefix.size() + 1) {
        const std::string name = cleanPath.substr(prefix.size() + 1);
        handleRemove(conn, name);
        return;
    }
    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("DELETE target not found\n");
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::replyStaticFile(const Server::TcpServer::TcpConnectionPtr& conn,
                                 const std::filesystem::path&               relativePath) {
    auto            target = staticDir_ / relativePath;
    std::error_code ec;
    if (!std::filesystem::exists(target, ec) || !std::filesystem::is_regular_file(target, ec)) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kNotFound, "Not Found");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Static file missing\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    std::ifstream      ifs(target, std::ios::binary);
    std::ostringstream oss;
    oss << ifs.rdbuf();  // 将文件的内容传递到oss

    HttpResponse resp;
    resp.setContentType("text/html; charset=utf-8");
    resp.setBody(oss.str());
    conn->send(resp.serialize(false));
    conn->shutdown();  // 发送完数据后会断开连接
}

void HttpServer::replyFileList(const Server::TcpServer::TcpConnectionPtr& conn) {
    std::vector<std::string> names;
    std::error_code          ec;
    std::filesystem::create_directories(storageDir_, ec);
    if (!ec) {
        for (const auto& entry : std::filesystem::directory_iterator(storageDir_, ec)) {
            if (entry.is_regular_file()) {
                names.emplace_back(entry.path().filename().string());
            }
        }
    }
    std::sort(names.begin(), names.end());

    std::ostringstream json;
    json << "{\"files\":[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            json << ',';
        }
        json << '\"' << escapeJson(names[i]) << '\"';
    }
    json << "]}";

    HttpResponse resp;
    resp.setContentType("application/json; charset=utf-8");
    resp.setBody(json.str());
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::replyDownload(const Server::TcpServer::TcpConnectionPtr& conn,
                               std::string_view                           fileName) {
    const std::string safeName = sanitizeFilename(urlDecode(fileName));
    auto              target   = storageDir_ / safeName;
    std::error_code   ec;
    if (safeName.empty() || !std::filesystem::exists(target, ec) ||
        !std::filesystem::is_regular_file(target, ec)) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kNotFound, "Not Found");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("File not found\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    std::ifstream      ifs(target, std::ios::binary);
    std::ostringstream oss;
    oss << ifs.rdbuf();

    HttpResponse resp;
    resp.setStatus(StatusCode::kOk, "OK");
    resp.setContentType("application/octet-stream");
    resp.setHeader("Content-Disposition", "attachment; filename=\"" + safeName + "\"");
    resp.setBody(oss.str());
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::handleUpload(const Server::TcpServer::TcpConnectionPtr& conn,
                              const HttpRequest&                         req) {
    auto it = req.headers.find("x-filename");
    if (it == req.headers.end()) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kBadRequest, "Bad Request");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Missing X-Filename header\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }
    const std::string safeName = sanitizeFilename(urlDecode(it->second));
    if (safeName.empty() || req.body.empty()) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kBadRequest, "Bad Request");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Empty filename or body\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(storageDir_, ec);
    auto          target = storageDir_ / safeName;
    std::ofstream ofs(target, std::ios::binary);
    if (!ofs.is_open()) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kInternalServerError, "Internal Server Error");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Failed to store file\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }
    ofs.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));

    HttpResponse resp;
    resp.setStatus(StatusCode::kCreated, "Created");
    resp.setContentType("application/json; charset=utf-8");
    resp.setBody("{\"status\":\"ok\"}");
    conn->send(resp.serialize(false));
    conn->shutdown();
}

void HttpServer::handleRemove(const Server::TcpServer::TcpConnectionPtr& conn,
                              std::string_view                           fileName) {
    const std::string safeName = sanitizeFilename(urlDecode(fileName));
    if (safeName.empty()) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kBadRequest, "Bad Request");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Invalid filename\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    auto            target = storageDir_ / safeName;
    std::error_code ec;
    if (!std::filesystem::exists(target, ec)) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kNotFound, "Not Found");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("File not found\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    std::filesystem::remove(target, ec);
    if (ec) {
        HttpResponse resp;
        resp.setStatus(StatusCode::kInternalServerError, "Internal Server Error");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Failed to delete file\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }

    HttpResponse resp;
    resp.setContentType("application/json; charset=utf-8");
    resp.setBody("{\"status\":\"deleted\"}");
    conn->send(resp.serialize(false));
    conn->shutdown();
}

}  // namespace Http
