#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <string>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <utility>

#undef ERROR  // Undefine Windows ERROR macro

enum class LogLevel_Type {
    NONE = 0,
    ERROR = 1,
    INFO = 2,
    DEBUG = 3,
    SLOPYAP = 4
};

inline LogLevel_Type currentLogLevel = LogLevel_Type::INFO;

// Utility function declaration
std::string wstring_to_string(const std::wstring& wstr);

namespace Logging {
    namespace detail {
        inline void format_impl(std::ostringstream& oss, const std::string& fmt) {
            oss << fmt;
        }

        template <typename T, typename... Args>
        void format_impl(std::ostringstream& oss, const std::string& fmt, T&& value, Args&&... args) {
            size_t pos = fmt.find("{}");
            if (pos != std::string::npos) {
                oss << fmt.substr(0, pos) << value;
                format_impl(oss, fmt.substr(pos + 2), std::forward<Args>(args)...);
            } else {
                oss << fmt;
            }
        }
    }
    
    inline void SetLogLevel(LogLevel_Type level) {
        currentLogLevel = level;
    }

    inline bool shouldLog(LogLevel_Type level) {
        return level <= currentLogLevel;
    }

    template <typename... Args>
    void log(LogLevel_Type level, const std::string& fmt, Args&&... args) {
        if (!shouldLog(level)) {
            return;
        }
        std::ostringstream oss;
        detail::format_impl(oss, fmt, std::forward<Args>(args)...);
        std::cout << oss.str() << '\n';
    }

    template <typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        log(LogLevel_Type::INFO, "[INFO] " + fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        log(LogLevel_Type::ERROR, "[ERROR] " + fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        log(LogLevel_Type::DEBUG, "[DEBUG] " + fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void slopyap(const std::string& fmt, Args&&... args) {
        log(LogLevel_Type::SLOPYAP, "[SLOPYAP] " + fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    std::string format(const std::string& fmt, Args&&... args) {
        std::ostringstream oss;
        detail::format_impl(oss, fmt, std::forward<Args>(args)...);
        return oss.str();
    }

    template <typename... Args>
    void panic(const std::string& fmt, Args&&... args) {
        error("[PANIC] " + format(fmt, std::forward<Args>(args)...));
        std::exit(1);
    }
}

#endif // LOGGING_HPP
