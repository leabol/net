#include "http/HttpServer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "Log.hpp"
#include "http/HttpParser.hpp"
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

    LOG_INFO("HttpServer initializing: storageDir={}, staticDir={}",
             storageDir_.string(),
             staticDir_.string());

    std::error_code ec;
    std::filesystem::create_directories(storageDir_, ec);
    if (ec) {
        LOG_WARN("failed to ensure storage dir {}: {}", storageDir_.string(), ec.message());
    } else {
        LOG_DEBUG("storage directory created/verified: {}", storageDir_.string());
    }

    server_.setConnectionCallback(
        [this](const Server::TcpServer::TcpConnectionPtr& conn) { this->onConnection(conn); });
    server_.setMessageCallback([this](const Server::TcpServer::TcpConnectionPtr& conn,
                                      std::string& data) { this->onMessage(conn, data); });
}

void HttpServer::start() {
    LOG_INFO("HttpServer starting...");
    server_.start();
    LOG_INFO("HttpServer started successfully");
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
    LOG_TRACE("fd={} received {} bytes", conn->fd(), data.size());
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;

    auto state = parser.feed(data);
    while (true) {
        if (state == HttpParser::FeedState::COMPLETE) {
            const HttpRequest& req = parser.getRequest();
            handleRequest(conn, req);
            parser.resetParser(true);
            state = parser.feed("");  // 继续尝试解析缓冲里的后续请求
            continue;
        }
        if (state == HttpParser::FeedState::ERROR) {
            LOG_ERROR("HTTP parsing error on fd={}, closing connection", conn->fd());
            conn->shutdown();
            break;
        }
        break;  // NEED_MORE
    }
}
void HttpServer::handleRequest(const Server::TcpServer::TcpConnectionPtr& conn,
                               const HttpRequest&                         req) {
    LOG_INFO("fd={} {} {}", conn->fd(), req.method, req.path);
    LOG_DEBUG("fd={} request details: version={}, headers={}, body_size={}",
              conn->fd(),
              req.version,
              req.headers.size(),
              req.body.size());

    if (req.method == "GET") {
        handleGet(conn, req);
    } else if (req.method == "POST") {
        handlePost(conn, req);
    } else if (req.method == "DELETE") {
        handleDelete(conn, req);
    } else {
        LOG_WARN("fd={} unsupported HTTP method: {}", conn->fd(), req.method);
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
    LOG_DEBUG("fd={} GET cleanPath={}", conn->fd(), cleanPath);

    if (cleanPath == "/" || cleanPath == "/index.html") {
        LOG_DEBUG("fd={} serving index.html", conn->fd());
        replyStaticFile(conn, "index.html");
        return;
    }

    // 获取文件列表
    constexpr std::string_view prefix = "/api/files";
    if (cleanPath == prefix) {
        LOG_DEBUG("fd={} listing files", conn->fd());
        replyFileList(conn);
        return;
    }
    // 获取具体文件
    if (cleanPath.rfind(prefix, 0) == 0 && cleanPath.size() > prefix.size() + 1) {
        const std::string name = cleanPath.substr(prefix.size() + 1);
        LOG_DEBUG("fd={} downloading file: {}", conn->fd(), name);
        replyDownload(conn, name);
        return;
    }

    LOG_WARN("fd={} GET path not found: {}", conn->fd(), cleanPath);
    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("Resource not found\n");
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::handlePost(const Server::TcpServer::TcpConnectionPtr& conn,
                            const HttpRequest&                         req) {
    const std::string cleanPath = stripQuery(req.path);
    LOG_DEBUG("fd={} POST cleanPath={}", conn->fd(), cleanPath);

    if (cleanPath == "/api/files") {
        LOG_DEBUG("fd={} uploading file", conn->fd());
        handleUpload(conn, req);
        return;
    }
    LOG_WARN("fd={} POST path not found: {}", conn->fd(), cleanPath);
    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("POST target not found\n");
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::handleDelete(const Server::TcpServer::TcpConnectionPtr& conn,
                              const HttpRequest&                         req) {
    const std::string          cleanPath = stripQuery(req.path);
    constexpr std::string_view prefix    = "/api/files";
    LOG_DEBUG("fd={} DELETE cleanPath={}", conn->fd(), cleanPath);

    if (cleanPath.rfind(prefix, 0) == 0 && cleanPath.size() > prefix.size() + 1) {
        const std::string name = cleanPath.substr(prefix.size() + 1);
        LOG_DEBUG("fd={} deleting file: {}", conn->fd(), name);
        handleRemove(conn, name);
        return;
    }
    LOG_WARN("fd={} DELETE path not found: {}", conn->fd(), cleanPath);
    HttpResponse resp;
    resp.setStatus(StatusCode::kNotFound, "Not Found");
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("DELETE target not found\n");
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::replyStaticFile(const Server::TcpServer::TcpConnectionPtr& conn,
                                 const std::filesystem::path&               relativePath) {
    auto            target = staticDir_ / relativePath;
    std::error_code ec;
    LOG_TRACE("fd={} serving static file: {}", conn->fd(), target.string());

    if (!std::filesystem::exists(target, ec) || !std::filesystem::is_regular_file(target, ec)) {
        LOG_ERROR("fd={} static file not found: {}", conn->fd(), target.string());
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
    LOG_DEBUG("fd={} served static file {} ({} bytes)",
              conn->fd(),
              relativePath.string(),
              oss.str().size());

    HttpResponse resp;
    resp.setContentType("text/html; charset=utf-8");
    resp.setBody(oss.str());
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
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
        LOG_DEBUG("fd={} file list: {} files found", conn->fd(), names.size());
    } else {
        LOG_ERROR(
            "fd={} failed to list files in {}: {}", conn->fd(), storageDir_.string(), ec.message());
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
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::replyDownload(const Server::TcpServer::TcpConnectionPtr& conn,
                               std::string_view                           fileName) {
    const std::string safeName = sanitizeFilename(urlDecode(fileName));
    auto              target   = storageDir_ / safeName;
    std::error_code   ec;
    LOG_TRACE("fd={} download request: fileName={}, safeName={}", conn->fd(), fileName, safeName);

    if (safeName.empty() || !std::filesystem::exists(target, ec) ||
        !std::filesystem::is_regular_file(target, ec)) {
        LOG_WARN("fd={} file not found for download: {}", conn->fd(), safeName);
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
    LOG_INFO("fd={} downloaded file: {} ({} bytes)", conn->fd(), safeName, oss.str().size());

    HttpResponse resp;
    resp.setStatus(StatusCode::kOk, "OK");
    resp.setContentType("application/octet-stream");
    resp.setHeader("Content-Disposition", "attachment; filename=\"" + safeName + "\"");
    resp.setBody(oss.str());
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::handleUpload(const Server::TcpServer::TcpConnectionPtr& conn,
                              const HttpRequest&                         req) {
    auto it = req.headers.find("x-filename");
    if (it == req.headers.end()) {
        LOG_WARN("fd={} upload missing X-Filename header", conn->fd());
        HttpResponse resp;
        resp.setStatus(StatusCode::kBadRequest, "Bad Request");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Missing X-Filename header\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }
    const std::string safeName = sanitizeFilename(urlDecode(it->second));
    LOG_TRACE("fd={} upload: originalName={}, safeName={}, bodySize={}",
              conn->fd(),
              it->second,
              safeName,
              req.body.size());

    if (safeName.empty() || req.body.empty()) {
        LOG_WARN("fd={} upload failed: empty filename or body", conn->fd());
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
        LOG_ERROR("fd={} failed to open file for writing: {}", conn->fd(), target.string());
        HttpResponse resp;
        resp.setStatus(StatusCode::kInternalServerError, "Internal Server Error");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Failed to store file\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }
    ofs.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
    LOG_INFO("fd={} uploaded file: {} ({} bytes)", conn->fd(), safeName, req.body.size());

    HttpResponse resp;
    resp.setStatus(StatusCode::kCreated, "Created");
    resp.setContentType("application/json; charset=utf-8");
    resp.setBody("{\"status\":\"ok\"}");
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

void HttpServer::handleRemove(const Server::TcpServer::TcpConnectionPtr& conn,
                              std::string_view                           fileName) {
    const std::string safeName = sanitizeFilename(urlDecode(fileName));
    LOG_TRACE("fd={} delete request: fileName={}, safeName={}", conn->fd(), fileName, safeName);

    if (safeName.empty()) {
        LOG_WARN("fd={} delete failed: invalid filename", conn->fd());
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
        LOG_WARN("fd={} delete failed: file not found: {}", conn->fd(), safeName);
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
        LOG_ERROR("fd={} failed to delete file {}: {}", conn->fd(), safeName, ec.message());
        HttpResponse resp;
        resp.setStatus(StatusCode::kInternalServerError, "Internal Server Error");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("Failed to delete file\n");
        conn->send(resp.serialize(false));
        conn->shutdown();
        return;
    }
    LOG_INFO("fd={} deleted file: {}", conn->fd(), safeName);

    HttpResponse resp;
    resp.setContentType("application/json; charset=utf-8");
    resp.setBody("{\"status\":\"deleted\"}");
    conn->send(resp.serialize(true));
    auto& ctx    = contexts_[conn->fd()];
    auto& parser = ctx.parser;
    if (!parser.isKeepAlive()) {
        conn->shutdown();
    }
}

}  // namespace Http
