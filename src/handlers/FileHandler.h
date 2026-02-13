#pragma once

#include <filesystem>
#include <string>

#include "handlers/RequestHandler.h"
#include "utils/FileCache.h"

class FileHandler : public RequestHandler {
public:
    FileHandler(std::string docRoot, FileCache* cache);

    http::HttpResponse handle(const http::HttpRequest& request) override;

private:
    bool sanitizeAndResolvePath(const std::string& uri, std::filesystem::path& outPath) const;
    std::string detectMimeType(const std::filesystem::path& path) const;

    std::filesystem::path docRoot_;
    std::filesystem::path canonicalDocRoot_;
    FileCache* cache_;
    std::size_t maxFileSize_{10 * 1024 * 1024};
};
