#include "http/HttpRequest.h"

#include <algorithm>
#include <cctype>

namespace http {
namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}  // namespace

std::string HttpRequest::getHeader(const std::string& key) const {
    const auto it = headers.find(toLower(key));
    if (it == headers.end()) {
        return "";
    }
    return it->second;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    return headers.find(toLower(key)) != headers.end();
}

std::size_t HttpRequest::getContentLength() const {
    const std::string raw = getHeader("content-length");
    if (raw.empty()) {
        return 0;
    }
    try {
        return static_cast<std::size_t>(std::stoull(raw));
    } catch (...) {
        return 0;
    }
}

bool HttpRequest::isKeepAlive() const {
    const std::string connection = toLower(getHeader("connection"));
    if (!connection.empty()) {
        return connection == "keep-alive";
    }
    return version == "HTTP/1.1";
}

}  // namespace http
