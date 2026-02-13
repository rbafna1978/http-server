#pragma once

#include <mutex>
#include <string>

class Logger {
public:
    enum class Level {
        Debug = 0,
        Info = 1,
        Error = 2,
    };

    void setLogLevel(Level level);
    void log(const std::string& message, Level level = Level::Info);
    void error(const std::string& message);

private:
    std::string timestamp() const;

    std::mutex mutex_;
    Level minLevel_{Level::Info};
};
