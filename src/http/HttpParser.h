#pragma once

#include <cstddef>

#include "http/HttpRequest.h"

namespace http {

class HttpParser {
public:
    bool parse(const char* buffer, std::size_t len, HttpRequest& request) const;
    bool parse(const char* buffer, std::size_t len, HttpRequest& request, std::size_t& consumedBytes) const;
};

}  // namespace http
