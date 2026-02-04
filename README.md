# Multithreaded HTTP Server

> High-performance HTTP/1.1 web server built from scratch in C++ using POSIX sockets

## 🚧 Status: Work in Progress

This project is currently under active development. Expected completion: **February 2026**

## 📋 Overview

A production-grade HTTP/1.1 web server implemented from scratch in C++ using POSIX sockets, featuring a custom thread pool with work-stealing queue and lock-free request parsing. Built to handle 1000+ concurrent connections with high throughput.

## ✨ Planned Features

- [x] Project architecture design
- [x] POSIX socket setup
- [ ] HTTP/1.1 protocol parser
- [ ] Thread pool with work-stealing queue
- [ ] Connection pooling & keep-alive
- [ ] Static file serving
- [ ] Request routing
- [ ] Lock-free request parsing
- [ ] Performance benchmarking with ApacheBench

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────┐
│              Client Connections                  │
│  (HTTP Requests via TCP)                        │
└────────────┬────────────────────────────────────┘
             │
      ┌──────▼──────┐
      │  Acceptor   │  ← Main thread accepts connections
      │   Thread    │
      └──────┬──────┘
             │
      ┌──────▼──────────────────────────────────┐
      │         Thread Pool                      │
      │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
      │  │ W1   │ │ W2   │ │ W3   │ │ W4   │  │
      │  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘  │
      │     │        │        │        │        │
      │  ┌──▼────────▼────────▼────────▼─────┐ │
      │  │    Work-Stealing Task Queue       │ │
      │  └───────────────────────────────────┘ │
      └──────────────────────────────────────────┘
             │
      ┌──────▼──────┐
      │   Request   │  ← Parse HTTP, route, respond
      │   Handler   │
      └──────┬──────┘
             │
      ┌──────▼──────┐
      │  Response   │  ← Send response back
      └─────────────┘
```

**Key Components:**
- **Acceptor Thread:** Accepts new connections, delegates to workers
- **Worker Threads:** Process requests from work queue
- **Work-Stealing Queue:** Lock-free deque for load balancing
- **HTTP Parser:** Zero-copy parsing of HTTP requests
- **File Cache:** In-memory cache for frequently accessed static files

## 🛠️ Tech Stack

- **Language:** C++ 17
- **Networking:** POSIX sockets (Berkeley sockets)
- **Threading:** `std::thread`, `std::mutex`, atomics
- **Build System:** CMake
- **Testing:** Google Test
- **Benchmarking:** ApacheBench (ab)

## 📖 Supported HTTP Features

**HTTP/1.1 Protocol:**
- GET and POST methods
- Persistent connections (keep-alive)
- Chunked transfer encoding
- Common headers (Host, User-Agent, Content-Length, etc.)

**Server Features:**
- Static file serving
- MIME type detection
- Directory listing (optional)
- 404/500 error pages
- Request logging

**Example Request/Response:**
```http
GET /index.html HTTP/1.1
Host: localhost:8080
Connection: keep-alive

HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 1234
Connection: keep-alive

<!DOCTYPE html>
<html>...
```

## 🎯 Performance Goals

- **Throughput:** 25K requests/sec on commodity hardware
- **Concurrent Connections:** 1000+ simultaneous connections
- **Latency:** Sub-10ms average response time for cached files
- **CPU Efficiency:** Near-linear scaling with thread count

## 🚀 Thread Pool Design

**Work-Stealing Queue:**
Each worker thread has its own deque. When idle, workers "steal" tasks from other workers' queues.

**Benefits:**
- Reduces lock contention (each thread owns its queue)
- Automatic load balancing
- Better cache locality (threads process related tasks)

**Implementation:**
```cpp
class WorkStealingQueue {
    std::deque<Task> tasks;
    std::mutex mutex;
    
public:
    void push(Task t);           // Owner pushes to back
    Task pop();                  // Owner pops from back (LIFO)
    Task steal();                // Other threads steal from front (FIFO)
};
```

## 📝 Implementation Roadmap

**Phase 1: Basic Server (Week 1)**
- Socket setup and accept loop
- Basic HTTP parser
- Single-threaded request handling

**Phase 2: Thread Pool (Week 2)**
- Worker thread creation
- Work-stealing queue implementation
- Connection delegation

**Phase 3: HTTP Features (Week 3)**
- Static file serving
- Persistent connections
- Request routing
- Error handling

**Phase 4: Optimization (Week 4)**
- Lock-free parsing
- File caching
- ApacheBench testing
- Performance tuning

## 🚀 Quick Start (Coming Soon)

Once complete, running the server:

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run
./http-server --port 8080 --threads 4 --root /var/www

# Test
curl http://localhost:8080/index.html
```

## 📊 Benchmark Results (Coming Soon)

Performance on commodity hardware (8-core CPU, 16GB RAM):

```bash
ab -n 100000 -c 1000 http://localhost:8080/index.html
```

| Metric | Target |
|--------|--------|
| Requests/sec | 25,000+ |
| Time per request (mean) | <1ms |
| Time per request (across all) | <40ms |
| Failed requests | 0 |

## 📚 Key Concepts

**epoll/select for I/O Multiplexing:**
Efficiently handle 1000+ connections without 1000 threads.

**Zero-Copy Parsing:**
Parse HTTP headers in-place without copying strings.

**Work-Stealing:**
Better load balancing than traditional work queues.

**Lock-Free Programming:**
Atomics and memory ordering for thread safety without locks.

## 📧 Contact

Questions? Reach out at bafnarishit@gmail.com

---

**Last Updated:** February 2026
