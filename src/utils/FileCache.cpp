#include "utils/FileCache.h"

#include <limits>

FileCache::FileCache(std::size_t maxSize) : maxSize_(maxSize == 0 ? 1 : maxSize) {}

std::optional<CachedFile> FileCache::get(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(path);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    it->second.lastAccess = std::chrono::steady_clock::now();
    return CachedFile{it->second.content, it->second.mimeType};
}

void FileCache::put(const std::string& path, std::string content, std::string mimeType) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.size() >= maxSize_) {
        evictLRU();
    }

    cache_[path] = CacheEntry{std::move(content), std::move(mimeType), std::chrono::steady_clock::now()};
}

void FileCache::evictLRU() {
    if (cache_.empty()) {
        return;
    }

    auto oldestIt = cache_.begin();
    auto oldestTs = oldestIt->second.lastAccess;

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.lastAccess < oldestTs) {
            oldestTs = it->second.lastAccess;
            oldestIt = it;
        }
    }

    cache_.erase(oldestIt);
}
