#pragma once

#include <string>
#include <unordered_map>

namespace http {

constexpr int HTTP_OK = 200;
constexpr int HTTP_BAD_REQUEST = 400;
constexpr int HTTP_TOO_MANY_REQUESTS = 429;
constexpr int HTTP_NOT_FOUND = 404;
constexpr int HTTP_METHOD_NOT_ALLOWED = 405;
constexpr int HTTP_INTERNAL_ERROR = 500;

inline const std::unordered_map<std::string, std::string> MIME_TYPES = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".txt", "text/plain"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"}
};

}  // namespace http
