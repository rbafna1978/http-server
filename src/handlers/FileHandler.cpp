#include "handlers/FileHandler.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "handlers/ErrorHandler.h"
#include "http/HttpConstants.h"

FileHandler::FileHandler(std::string docRoot, FileCache* cache)
    : docRoot_(std::move(docRoot)), cache_(cache) {
    if (!std::filesystem::exists(docRoot_)) {
        std::filesystem::create_directories(docRoot_);
    }
    canonicalDocRoot_ = std::filesystem::weakly_canonical(docRoot_);
}

http::HttpResponse FileHandler::handle(const http::HttpRequest& request) {
    if (request.method != "GET" && request.method != "HEAD") {
        return handlers::create405();
    }

    std::filesystem::path path;
    if (!sanitizeAndResolvePath(request.uri, path)) {
        return handlers::create404();
    }

    if (std::filesystem::is_directory(path)) {
        path /= "index.html";
    }

    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return handlers::create404();
    }

    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize > maxFileSize_) {
        return handlers::create500("File too large or unreadable");
    }

    const std::string pathKey = path.string();
    std::string content;
    std::string mimeType = detectMimeType(path);

    if (cache_ != nullptr) {
        auto cached = cache_->get(pathKey);
        if (cached.has_value()) {
            content = std::move(cached->content);
            mimeType = std::move(cached->mimeType);
        }
    }

    if (content.empty()) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return handlers::create500("Could not open file");
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();

        if (cache_ != nullptr) {
            cache_->put(pathKey, content, mimeType);
        }
    }

    http::HttpResponse resp;
    resp.setStatus(http::HTTP_OK, "OK");
    resp.setContentType(mimeType);
    resp.setHeader("Connection", request.isKeepAlive() ? "keep-alive" : "close");
    if (request.method == "HEAD") {
        resp.setHeader("Content-Length", std::to_string(content.size()));
        resp.setBody("");
    } else {
        resp.setBody(std::move(content));
    }
    return resp;
}

bool FileHandler::sanitizeAndResolvePath(const std::string& uri, std::filesystem::path& outPath) const {
    std::string cleanUri = uri;
    const std::size_t q = cleanUri.find('?');
    if (q != std::string::npos) {
        cleanUri = cleanUri.substr(0, q);
    }

    if (cleanUri.empty()) {
        cleanUri = "/";
    }

    if (cleanUri.find("..") != std::string::npos) {
        return false;
    }

    while (!cleanUri.empty() && cleanUri.front() == '/') {
        cleanUri.erase(cleanUri.begin());
    }

    std::filesystem::path candidate = canonicalDocRoot_ / cleanUri;
    std::filesystem::path canonicalCandidate = std::filesystem::weakly_canonical(candidate);
    std::error_code ec;
    std::filesystem::path rel = std::filesystem::relative(canonicalCandidate, canonicalDocRoot_, ec);
    if (ec) {
        return false;
    }
    if (rel.empty()) {
        outPath = canonicalCandidate;
        return true;
    }
    const auto relStr = rel.generic_string();
    if (relStr == ".." || relStr.rfind("../", 0) == 0) {
        return false;
    }

    outPath = canonicalCandidate;
    return true;
}

std::string FileHandler::detectMimeType(const std::filesystem::path& path) const {
    const std::string ext = path.extension().string();
    const auto it = http::MIME_TYPES.find(ext);
    if (it != http::MIME_TYPES.end()) {
        return it->second;
    }
    return "application/octet-stream";
}
