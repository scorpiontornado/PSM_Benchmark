#ifndef MATCHING_TASKPOOL_H
#define MATCHING_TASKPOOL_H

#include "graph/graph.h"
#include "Args.h"
#include "utility/QFliter.h"
#include "utility/statistics/Logs.h"
#include "utility/statistics/intTimer.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <vector>
#include <queue>
#include <bitset>
#include <functional>
#include <limits>
#include <algorithm>
#include <sstream>

#define MAX_THREADS_COUNT 44
#define MINIMUM_TASK_LENGTH  2

// for global variables, use shallow copy
// for local variables, use deep copy

// for main thread

// 所有的INIT的时候都是 thread_num 个任务，而不是 capacity 个任务

enum ScheduleType {
    STATIC = 0,   // capacity = 2 * thread_num for Shixuan
    // BUSY2IDLE = 1,  // capacity == thread_num for for Shixuan
    TIMEOUT = 1,    // capacity same as DEFAULT for Shixuan
    BUSY2IDLENOSTOP = 2, //
    BUSY2IDLEDEPTHSTOP = 3, //
    STATICWORKLOAD = 4,
};

enum SplitType {
    // SHIXUAN = 0,    // 随意在某一层分
    // BENU = 1,       // 只 init，后面不分
    LINEAR = 1,     // 线性分
    WORKLOAD = 2,   // 根据workload分
    UPPERONE = 3,   // 只分一个candidate出去
    LAYER = 4,  // 尽可能把这一层全部分掉，但是相比于 pRI/VF系列 那种，不能把所有中间状态全部维护，加了一个 CAPACITY 的限制
};

// task only contains necessary information like indices
class TaskSlot {
public:
    TaskSlot() = default;
    explicit TaskSlot(LocalArgs* l_args) = delete;
    
    // Copy constructor
    TaskSlot(const TaskSlot& task);
    
    // Move constructor
    TaskSlot(TaskSlot&& task);
    
    // Copy assignment operator
    TaskSlot& operator=(const TaskSlot& task);
    
    // Move assignment operator
    TaskSlot& operator=(TaskSlot&& task);
    
    ~TaskSlot();

    bool checkSplitTimeout();

public:
    static GLOBALARGS* g_args;
    LocalArgs* l_args{nullptr}; // Initialize to nullptr
    // int id_unit{-1};
    int64_t start_task_time{0};
    int64_t timeThreshold{100'000'000}; // default by 0.1s
};

class TaskPool {
public:
    TaskPool(uint64_t num_threads, uint64_t queue_capacity);
    ~TaskPool();
    bool Empty();
    std::size_t Size();

    void CancelReserve();
    bool PreReserve(bool layerAll, int& containerCount, int left_idx_count);

    bool Push(TaskSlot* task);
    bool Push(const TaskSlot& task);
    bool Push(TaskSlot&& task);
    int Pop(TaskSlot& task);
    bool isFull();
    void setExtendable(ui* extendable, TreeNode* tree, int max_depth);

    void PrintStatistics();

    void singleThreadInitTasks(int id_unit = 0);
    void queryAddTasks(int id_unit);
    void candidateInitTasks(int id_unit = 0);
    void queryCandidateInitTasks();
    void staticInitTasks(int id_unit = 0);
    void initTasks();

    void splitTask(TaskSlot& parent_task, int down_depth);
    void splitTaskU(TaskSlot& parent_task, int down_depth);
    
    void handleSplitTime(std::chrono::_V2::steady_clock::time_point start_time_split);

    void addIdle();

    void subIdle();

    bool checkOverTime();

    bool checkOver();

    void layerDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count, 
                                std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec);

    void linearDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
                                std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec);

    void workloadDistribution(TaskSlot& parent_task, int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
                                std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec, UnitArgs* u_args, VertexID u);

    void upperoneDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
                                std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec);
    
    void printDistribution(int containerCount, std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui> idx_count_cur_vec);

public:
    std::mutex queue_mutex;
    std::condition_variable cv;
    uint64_t THREADS_NUMBER{1};
    int64_t QUEUE_CAPACITY{THREADS_NUMBER * 4};

    static std::function<void(TaskSlot&)> enum_method;
    std::queue<TaskSlot*> task_queue;
    int reserved_task_num{0};  // 预占容量计数
    int total_task_num{0};

    // thread message
    std::atomic<ui> idle_thread_num{0};

    ScheduleType schedule_type{ScheduleType::STATIC};
    SplitType split_type{SplitType::LINEAR};
};

#endif // MATCHING_TASKPOOL_H
