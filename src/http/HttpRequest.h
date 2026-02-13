#pragma once

#include <string>
#include <unordered_map>

namespace http {

class HttpRequest {
public:
    std::string method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    std::string getHeader(const std::string& key) const;
    bool hasHeader(const std::string& key) const;
    std::size_t getContentLength() const;
    bool isKeepAlive() const;

    const std::string& getMethod() const { return method; }
    const std::string& getUri() const { return uri; }
    const std::string& getVersion() const { return version; }
    const std::string& getBody() const { return body; }
};

}  // namespace http
