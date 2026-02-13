#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <sys/types.h>

class Socket {
public:
    Socket();
    explicit Socket(int fd);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    void bind(int port) const;
    void listen(int backlog = 128) const;
    Socket accept(std::string* peerIp = nullptr) const;

    ssize_t send(const char* data, std::size_t len) const;
    ssize_t recv(char* buffer, std::size_t size) const;

    void setNonBlocking() const;
    void setReuseAddr() const;
    void setKeepAlive() const;
    void setReceiveTimeoutSeconds(int seconds) const;
    void shutdownReadWrite() const;
    void close();

    int getFd() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }

private:
    static std::runtime_error makeError(const std::string& prefix);
    void closeIfValid();

    int fd_{-1};
};
