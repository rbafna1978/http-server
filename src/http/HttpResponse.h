#pragma once

#include <string>
#include <unordered_map>

namespace http {

class HttpResponse {
public:
    int statusCode{200};
    std::string statusMessage{"OK"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    void setStatus(int code, std::string message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(std::string content);
    void setContentType(const std::string& mimeType);
    std::string serialize() const;
};

}  // namespace http
