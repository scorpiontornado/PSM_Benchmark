#ifndef STATISTICS_LOG_H_
#define STATISTICS_LOG_H_

#include <thread>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <fstream>
#include <iostream>

#define LOG_LEVEL 0 // 控制日志级别，可以是 "sketch" 或 1 : "detail"

// 定义宏用于简化调用，并根据日志级别调整输出
#if defined(LOG_LEVEL) && LOG_LEVEL == 1
    #define LOG() LoggerStream(Log::getInstance(), Log::getInstance().getThreadNumber(), __FILE__, __LINE__)
#else
    #define LOG() LoggerStream(Log::getInstance(), Log::getInstance().getThreadNumber())
#endif

class Log {
public:
    // 获取 Log 类的单例实例

    static std::string LogFilePath;

    static Log& getInstance() {
        static Log instance;
        return instance;
    }

    static void setLogFilePath(std::string path) {
        Log::LogFilePath = path;
    }

    // 获取线程号，从1开始编号
    int getThreadNumber() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::thread::id thread_id = std::this_thread::get_id();
        if (thread_map_.find(thread_id) == thread_map_.end()) {
            thread_map_[thread_id] = ++thread_counter_;
        }
        return thread_map_[thread_id];
    }

    // 线程安全的输出操作
    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::ofstream log_file_;
        log_file_.open(LogFilePath, std::ios::out | std::ios::app);
        if (!log_file_) {
            std::cout << "Error: Failed to open log file at " << LogFilePath << std::endl;
            abort();
            return;
        }

        log_file_ << message;
        if (log_file_.fail()) {
            // std::cout << "Error: Failed to write to log file at " << LogFilePath << std::endl;
            abort();
        }

        // std::cout << message;

        log_file_.close();  // Try to close and check if it fails
        if (log_file_.fail()) {
            // std::cout << "Error: Failed to close log file at " << LogFilePath << std::endl;
            abort();
        }
    }

private:
    Log() : thread_counter_(0) {}
    ~Log() = default;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    std::mutex log_mutex;
    std::mutex mutex_;  // 用于保护 thread_map_ 的互斥锁
    std::unordered_map<std::thread::id, int> thread_map_;  // 线程 ID 到自定义编号的映射
    int thread_counter_;  // 线程计数器
};

// 定义宏用于简化调用


// 自定义的 LoggerStream 类，用于封装 ostringstream 和一次性输出
class LoggerStream {
public:
    LoggerStream(Log& logger, int thread_number, const char* file = nullptr, int line = 0)
        : logger_(logger) {
        // 构造前缀，包括线程号、文件名和行号（如果需要）
        oss_ << "[Thread " << thread_number << "] ";
        if (file && line) {
            oss_ << "[" << file << ":" << line << "] ";
        }
    }

    // 支持各种类型的输入，使用模板实现 << 操作符重载
    template <typename T>
    LoggerStream& operator<<(const T& msg) {
        oss_ << msg;
        return *this;
    }

    // 支持 std::endl 和其他流操作符
    LoggerStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        oss_ << manip;
        return *this;
    }

    // 在析构时将所有内容一次性输出
    ~LoggerStream() {
        logger_.log(oss_.str());
    }

private:
    Log& logger_;  // 引用到 Log 实例
    std::ostringstream oss_;  // 用于构建输出内容的 ostringstream
};

#endif //STATISTICS_LOG_H_