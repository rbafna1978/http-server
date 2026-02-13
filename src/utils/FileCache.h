#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

struct CachedFile {
    std::string content;
    std::string mimeType;
};

class FileCache {
public:
    explicit FileCache(std::size_t maxSize);

    std::optional<CachedFile> get(const std::string& path);
    void put(const std::string& path, std::string content, std::string mimeType);

private:
    struct CacheEntry {
        std::string content;
        std::string mimeType;
        std::chrono::steady_clock::time_point lastAccess;
    };

    void evictLRU();

    std::unordered_map<std::string, CacheEntry> cache_;
    std::mutex mutex_;
    std::size_t maxSize_;
};
