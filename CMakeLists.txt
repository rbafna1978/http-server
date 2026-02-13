cmake_minimum_required(VERSION 3.14)
project(HttpServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(SOURCES
    src/main.cpp
    src/server/Socket.cpp
    src/server/Acceptor.cpp
    src/server/HttpServer.cpp
    src/threadpool/ThreadPool.cpp
    src/threadpool/WorkStealingQueue.cpp
    src/http/HttpParser.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/handlers/RequestHandler.cpp
    src/handlers/FileHandler.cpp
    src/handlers/ErrorHandler.cpp
    src/utils/Logger.cpp
    src/utils/FileCache.cpp
)

add_executable(http-server ${SOURCES})
target_link_libraries(http-server PRIVATE pthread)
target_include_directories(http-server PRIVATE src)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(tests
        tests/test_parser.cpp
        tests/test_threadpool.cpp
        tests/test_server.cpp
        src/threadpool/ThreadPool.cpp
        src/threadpool/WorkStealingQueue.cpp
        src/http/HttpParser.cpp
        src/http/HttpRequest.cpp
        src/http/HttpResponse.cpp
        src/handlers/RequestHandler.cpp
        src/handlers/FileHandler.cpp
        src/handlers/ErrorHandler.cpp
        src/utils/FileCache.cpp
        src/server/Socket.cpp
    )

    target_include_directories(tests PRIVATE src)
    target_link_libraries(tests PRIVATE GTest::GTest GTest::Main pthread)
    add_test(NAME unit_tests COMMAND tests)
endif()
