#ifndef INTTIMER_H
#define INTTIMER_H

#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>

namespace intTimer {
  constexpr double TEMPNANOSECTOSEC(double elapsed_time) {
      return elapsed_time / 1000000000.0;
  }

  static int64_t getClockNano() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    return duration.count();
  }
}


// class Metrics {
// public:
//     void startTime(const std::string& key) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         start_times_[key] = std::chrono::steady_clock::now();
//     }

//     void stopTime(const std::string& key) {
//         const auto end_time = std::chrono::steady_clock::now();
//         std::lock_guard<std::mutex> lock(mutex_);
//         auto it = start_times_.find(key);
//         if (it != start_times_.end()) {
//             std::chrono::nanoseconds elapsed = 
//                 std::chrono::duration_cast<std::chrono::nanoseconds>(
//                     end_time - it->second
//                 );
//             addTime(key, elapsed);
//             start_times_.erase(it);
//         }
//     }

//     void addCount(const std::string& key, long long count) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         count_metrics_[key] += count;
//     }

//     std::chrono::nanoseconds getTime(const std::string& key) const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return time_metrics_.at(key);
//     }

//     long long getCount(const std::string& key) const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return count_metrics_.at(key);
//     }

//     const std::unordered_map<std::string, std::chrono::nanoseconds> getGlobalTimeMetrics() const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return time_metrics_;
//     }

//     const std::unordered_map<std::string, long long> getGlobalCountMetrics() const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return count_metrics_;
//     }

//     const std::unordered_map<std::string, std::chrono::nanoseconds> getLocalTimeMetrics() const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return time_metrics_;
//     }

//     const std::unordered_map<std::string, long long> getLocalCountMetrics() const {
//         std::lock_guard<std::mutex> lock(mutex_);
//         return count_metrics_;
//     }

// private:
//     void addTime(const std::string& key, std::chrono::nanoseconds time_ms) {
//         time_metrics_[key] += time_ms;
//     }

//     mutable std::mutex mutex_;  // 用于保护所有共享数据的互斥锁
//     std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> start_times_;
//     std::unordered_map<std::string, std::chrono::nanoseconds> time_metrics_;
//     std::unordered_map<std::string, long long> count_metrics_;
// };

#endif // INTTIMER_H