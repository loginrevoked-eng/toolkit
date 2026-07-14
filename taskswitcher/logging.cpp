#include "logging.hpp"
#include <string>       // std::string, std::wstring
#include <sstream>      // std::ostringstream
#include <iostream>     // std::cout
#include <utility>      // std::forward
#include <windows.h>    // WideCharToMultiByte, CP_UTF8 (Windows-specific)

std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}



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
    
    void SetLogLevel(LogLevel_Type level) {
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
    std::string format(const std::string& fmt, Args&&... args) {
        std::ostringstream oss;
        detail::format_impl(oss, fmt, std::forward<Args>(args)...);
        return oss.str();
    }

    template <typename... Args>
    void panic(const std::string& fmt, Args&&... args) {
        error(format("[PANIC] {}", fmt), std::forward<Args>(args)...);
        std::exit(1);
    }
    
}