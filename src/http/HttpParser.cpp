#include "http/HttpParser.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace http {
namespace {
constexpr std::size_t kMaxHeaderSize = 8 * 1024;
constexpr std::size_t kMaxBodySize = 10 * 1024 * 1024;
constexpr std::size_t kMaxUriLength = 2048;

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim(std::string_view sv) {
    std::size_t left = 0;
    while (left < sv.size() && std::isspace(static_cast<unsigned char>(sv[left]))) {
        ++left;
    }
    std::size_t right = sv.size();
    while (right > left && std::isspace(static_cast<unsigned char>(sv[right - 1]))) {
        --right;
    }
    return std::string(sv.substr(left, right - left));
}

}  // namespace

bool HttpParser::parse(const char* buffer, std::size_t len, HttpRequest& request) const {
    std::size_t ignored = 0;
    return parse(buffer, len, request, ignored);
}

bool HttpParser::parse(const char* buffer, std::size_t len, HttpRequest& request, std::size_t& consumedBytes) const {
    enum class ParseState {
        RequestLine,
        Headers,
        Body,
        Done
    };

    consumedBytes = 0;
    if (buffer == nullptr || len == 0) {
        return false;
    }

    std::string_view input(buffer, len);
    const std::size_t headerEnd = input.find("\r\n\r\n");
    if (headerEnd == std::string_view::npos) {
        if (len > kMaxHeaderSize) {
            throw std::runtime_error("Header section too large");
        }
        return false;
    }
    if (headerEnd + 4 > kMaxHeaderSize) {
        throw std::runtime_error("Header section too large");
    }

    ParseState state = ParseState::RequestLine;
    std::size_t cursor = 0;
    std::size_t bodyOffset = 0;
    std::size_t contentLength = 0;
    std::string currentHeader;
    request.headers.clear();

    while (state != ParseState::Done) {
        switch (state) {
            case ParseState::RequestLine: {
                const std::size_t lineEnd = input.find("\r\n", cursor);
                if (lineEnd == std::string_view::npos) {
                    return false;
                }

                std::string_view requestLine = input.substr(cursor, lineEnd - cursor);
                const std::size_t firstSpace = requestLine.find(' ');
                const std::size_t secondSpace =
                    requestLine.find(' ', firstSpace == std::string_view::npos ? firstSpace : firstSpace + 1);
                if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos) {
                    throw std::runtime_error("Malformed request line");
                }

                request.method = std::string(requestLine.substr(0, firstSpace));
                request.uri = std::string(requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1));
                request.version = std::string(requestLine.substr(secondSpace + 1));
                if (request.uri.size() > kMaxUriLength) {
                    throw std::runtime_error("URI too long");
                }
                if (request.version != "HTTP/1.1") {
                    throw std::runtime_error("Unsupported HTTP version");
                }

                cursor = lineEnd + 2;
                state = ParseState::Headers;
                break;
            }

            case ParseState::Headers: {
                while (cursor < headerEnd) {
                    const std::size_t next = input.find("\r\n", cursor);
                    if (next == std::string_view::npos || next > headerEnd) {
                        return false;
                    }

                    const std::string_view line = input.substr(cursor, next - cursor);
                    cursor = next + 2;

                    if (line.empty()) {
                        break;
                    }

                    if (line.front() == ' ' || line.front() == '\t') {
                        if (!currentHeader.empty()) {
                            request.headers[currentHeader] += " " + trim(line);
                        }
                        continue;
                    }

                    const std::size_t colon = line.find(':');
                    if (colon == std::string_view::npos) {
                        throw std::runtime_error("Malformed header line");
                    }

                    std::string key = toLower(trim(line.substr(0, colon)));
                    std::string value = trim(line.substr(colon + 1));
                    request.headers[key] = value;
                    currentHeader = key;
                }

                bodyOffset = headerEnd + 4;
                try {
                    contentLength = request.getContentLength();
                } catch (...) {
                    contentLength = 0;
                }
                if (contentLength > kMaxBodySize) {
                    throw std::runtime_error("Request body too large");
                }

                state = ParseState::Body;
                break;
            }

            case ParseState::Body: {
                if (len < bodyOffset + contentLength) {
                    return false;
                }
                request.body.assign(buffer + bodyOffset, contentLength);
                consumedBytes = bodyOffset + contentLength;
                state = ParseState::Done;
                break;
            }

            case ParseState::Done:
                break;
        }
    }

    return true;
}

}  // namespace http
