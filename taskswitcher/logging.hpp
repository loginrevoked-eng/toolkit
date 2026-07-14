#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <string>


enum class LogLevel_Type {
    NONE = 0,
    INFO = 1,
    ERROR = 2,
    DEBUG = 3
};

static LogLevel_Type currentLogLevel = LogLevel_Type::INFO;

// Forward declaration
class ostringstream;

// Utility function declaration
std::string wstring_to_string(const std::wstring& wstr);

namespace Logging {
    namespace detail {
        inline void format_impl(std::ostringstream& oss, const std::string& fmt);

        template <typename T, typename... Args>
        void format_impl(std::ostringstream& oss, const std::string& fmt, T&& value, Args&&... args);
    }
    inline bool shouldLog();
    void SetLogLevel(LogLevel_Type level = LogLevel_Type::INFO);

    template <typename... Args>
    void log(LogLevel_Type level, const std::string& fmt, Args&&... args);

    template <typename... Args>
    void info(const std::string& fmt, Args&&... args);

    template <typename... Args>
    void error(const std::string& fmt, Args&&... args);

    template <typename... Args>
    void debug(const std::string& fmt, Args&&... args);

    template <typename... Args>
    std::string format(const std::string& fmt, Args&&... args);
    
    template <typename... Args>
    void panic(const std::string& fmt, Args&&... args);
}

#endif // LOGGING_HPP