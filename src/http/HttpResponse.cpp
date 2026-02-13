#include "http/HttpResponse.h"

#include <sstream>

namespace http {

void HttpResponse::setStatus(int code, std::string message) {
    statusCode = code;
    statusMessage = std::move(message);
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void HttpResponse::setBody(std::string content) {
    body = std::move(content);
}

void HttpResponse::setContentType(const std::string& mimeType) {
    setHeader("Content-Type", mimeType);
}

std::string HttpResponse::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << statusCode << ' ' << statusMessage << "\r\n";

    std::unordered_map<std::string, std::string> allHeaders = headers;
    if (allHeaders.find("Content-Length") == allHeaders.end()) {
        allHeaders["Content-Length"] = std::to_string(body.size());
    }
    if (allHeaders.find("Connection") == allHeaders.end()) {
        allHeaders["Connection"] = "close";
    }

    for (const auto& [key, value] : allHeaders) {
        out << key << ": " << value << "\r\n";
    }
    out << "\r\n";
    out << body;
    return out.str();
}

}  // namespace http
