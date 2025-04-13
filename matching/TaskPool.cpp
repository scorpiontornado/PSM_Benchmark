#include "TaskPool.h"

GLOBALARGS* TaskSlot::g_args = nullptr;

// Definition of TaskSlot
TaskSlot::TaskSlot(const TaskSlot& task) {
    // LOG() << "call TaskSlot's copy constructor" << std::endl;
    l_args = new LocalArgs(*(task.l_args));
}

TaskSlot::TaskSlot(TaskSlot&& task) : l_args(task.l_args) {
    // LOG() << "call TaskSlot's move constructor" << std::endl;
    task.l_args = nullptr;
}

TaskSlot&
TaskSlot::operator=(const TaskSlot& task) {
    // LOG() << "call TaskSlot's copy assignment" << std::endl;
    if (this != &task) { // Check for self-assignment
        delete l_args; // Clean up existing resource
        l_args = new LocalArgs(*task.l_args); // Copy resource
    }
    return *this;
}

TaskSlot&
TaskSlot::operator=(TaskSlot&& task) {
    // LOG() << "call TaskSlot's move assignment" << std::endl;
    if (this != &task) { // Check for self-assignment
        // LOG() << "original l_args == " << l_args << std::endl;
        delete l_args; // Clean up existing resource
        // LOG() << "copied task.l_args == " << task.l_args << std::endl;
        l_args = task.l_args; // Transfer ownership
        task.l_args = nullptr; // Nullify the source object
    }
    return *this;
}

TaskSlot::~TaskSlot() {
    // NOTE: global_args will be deleted in main thread
    // delete global_args;
    // LOG() << "call TaskSlot's destructor" << std::endl;
    delete l_args;
}

bool
TaskSlot::checkSplitTimeout() {
    
}

// Definition of TaskPool

TaskPool::TaskPool(uint64_t num_threads, uint64_t queue_capacity)
    : THREADS_NUMBER(num_threads), QUEUE_CAPACITY(queue_capacity) 
{ }

TaskPool::~TaskPool() {
    if (TaskSlot::g_args->overtime.load()) {
        while (!task_queue.empty()) {
            delete task_queue.front();  // 删除指向的 TaskSlot 对象
            task_queue.pop();           // 移除指针本身
        }            
    }
    // 如果没超时，不应该出现析构taskpool但是还有任务的情况
    while (!task_queue.empty()) {
        LOG() << "Error: TaskPool is not empty, waiting for all tasks to finish." << std::endl;
        abort();
    }
};

bool
TaskPool::Push(const TaskSlot& task) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    TaskSlot* new_task = new TaskSlot(task);
    task_queue.push(new_task);
    // LOG() << "PUSH size == " << Size() << std::endl;
    return true;
}

bool
TaskPool::Push(TaskSlot&& task) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    // LOG() << "PUSH 2" << std::endl;
    TaskSlot* new_task = new TaskSlot(std::move(task));
    task_queue.push(new_task);
    // LOG() << "PUSH size == " << Size() << std::endl;
    delete &task;

    // int cur_depth = new_task->l_args->upper_depth;
    // LOG() << "Push task at depth " << cur_depth << ", new idx_count[" << cur_depth << "] = "
    //     << new_task->l_args->idx_count[cur_depth] << std::endl;

    return true;
}

int
TaskPool::Pop(TaskSlot& task) {
    // TODO: chech if the std::move takes effect
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (Empty()) {
        // LOG() << "no task available" << std::endl;
        return 0;
    }
    task = std::move(*task_queue.front());
    // LOG() << "task_queue.front() : " << task_queue.front() << std::endl;
    // LOG() << "task_queue.front().l_args : " << task_queue.front()->l_args << std::endl;
    delete task_queue.front();
    task_queue.pop();
    reserved_task_num--; // 对 reserved_task_num 进行原子递减
    // LOG() << "reserved_task_num == " << reserved_task_num << std::endl;
    total_task_num += 1;
    // LOG() << "POP size == " << Size() << std::endl;
    return total_task_num;
}

// bool
// TaskPool::isFull() {
//     return task_queue.size() >= QUEUE_CAPACITY;
// }

bool
TaskPool::Empty() {
    return task_queue.empty();
}

std::size_t
TaskPool::Size() {
    return task_queue.size();
}

void
TaskPool::CancelReserve() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    reserved_task_num--;
}

bool
TaskPool::isFull() {
    return reserved_task_num >= QUEUE_CAPACITY;
}

/**
 * 在构造 TaskSlot 之前调用，检查 reserved_task_num 是否小于 QUEUE_CAPACITY
    * 如果小于，则增加 1，返回 true 允许创建任务；
    * 否则返回 false，直接拒绝任务。
 * 
 */
bool
TaskPool::PreReserve(bool layerAll, int& containerCount, int left_idx_count) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (reserved_task_num >= QUEUE_CAPACITY) {
        // LOG() << "Full: TaskPool is full, PreReserve failed." << std::endl;
        return false;
    }

    /**
     * TODO: containerCount 要改一下，这里的 QUEUE_CAPACITY 太大了
     * 
     */

    if (layerAll) {
        if (schedule_type == ScheduleType::STATIC || schedule_type == ScheduleType::STATICWORKLOAD) {
            containerCount = left_idx_count;
        } else {
            containerCount = std::min(QUEUE_CAPACITY - reserved_task_num + 1, static_cast<int64_t>(left_idx_count));
        }
        // containerCount = std::min(QUEUE_CAPACITY - reserved_task_num + 1, static_cast<int64_t>(left_idx_count));
        // containerCount = QUEUE_CAPACITY - reserved_task_num + 1;
        reserved_task_num += (containerCount - 1);
        // LOG() << "Add reserved_task_num: LayerAll, succeed : "
        //         << "reserved_task_num == " << reserved_task_num
        //         << ", QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
    } else {
        containerCount = 2;
        reserved_task_num += 1;
        // LOG() << "Add reserved_task_num: Others, succeed : "
        //         << "reserved_task_num == " << reserved_task_num
        //         << ", QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
    }
    // LOG() << "QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
    // LOG() << "reserved_task_num == " << reserved_task_num << std::endl;
    // LOG() << "left_idx_count == " << left_idx_count << std::endl;
    // LOG() << "containerCount == " << containerCount << std::endl;

    return true;
}

void
TaskPool::addIdle() {
    idle_thread_num++;
}

void
TaskPool::subIdle() {
    idle_thread_num--;
}

bool
TaskPool::checkOverTime() {
    if (intTimer::getClockNano() >= TaskSlot::g_args->time_limit) {
        TaskSlot::g_args->overtime = true;
        return true;
    }
    return false;
}

bool
TaskPool::checkOver() {
    return idle_thread_num >= THREADS_NUMBER || TaskSlot::g_args->overtime.load();
}

void
TaskPool::setExtendable(ui* extendable, TreeNode* tree, int max_depth) {
    for (int i = 0; i < max_depth; ++i) {
        extendable[i] = tree[i].bn_count_;
    }
}

void
TaskPool::singleThreadInitTasks(int id_unit) {
    TaskSlot* task = new TaskSlot();
    LocalArgs*& l_args = task->l_args;
    l_args = new LocalArgs();
    l_args->id_unit = id_unit;
    l_args->allocateBuffer(TaskSlot::g_args);
    l_args->upper_depth = 0;
    l_args->cur_depth = 0;
    int cur_depth = l_args->cur_depth;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[l_args->id_unit];
    VertexID start_vertex = u_args->order[cur_depth];

    ui candi_count = u_args->candidates_count[u_args->order[0]];

// NOTE: 这里是专门针对DPiso的，后面要想办法分离开
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    VertexID cur_u = u_args->order[cur_depth];
    l_args->l_order[0] = u_args->order[0];
    l_args->u_idx_count[cur_u] = candi_count;

    // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
    for (ui ii = 0; ii < l_args->u_idx_count[cur_u]; ii++) {
        l_args->valid_candidate_idx[cur_u][ii] = ii;
    }
    setExtendable(l_args->extendable, u_args->tree, u_args->vertices_count);
} else if (USING_CANDIDATE_INDEX_FLAG) {
    l_args->idx_count[cur_depth] = candi_count;

    // 这里有错，这个 valid_candidate_idx 是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
    for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
        l_args->valid_candidate_idx[cur_depth][ii] = ii;
    }

} else if (!USING_CANDIDATE_INDEX_FLAG) {
    l_args->idx_count[cur_depth] = candi_count;
    for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
        l_args->valid_candidate[cur_depth][ii] = u_args->candidates[start_vertex][ii];
    }
}

if (FROZEN_SET_FLAG) {
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>>& FUValidCandidateIdx = l_args->FUValidCandidateIdx;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui>>& FUValidCandidatesCount = l_args->FUValidCandidatesCount;
    std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
    for (const auto& [fu, fns] : FU_fns) {
        FUValidCandidateIdx[fu] = std::unordered_map<VertexID, ui*>();
        FUValidCandidatesCount[fu] = std::unordered_map<VertexID, ui>();
        for (VertexID fn : fns) {
            FUValidCandidateIdx[fu][fn] = new ui[u_args->candidates_count[fu]];
        }
    }
}

    l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
    // LOG() << "l_limit_num == " << l_args->l_limit_num << std::endl;
// #endif
    int tmp;
    PreReserve(false, tmp, 1);

    if (task_queue.size() >= QUEUE_CAPACITY) {
        LOG() << "Error: TaskPool is full, init error" << std::endl;
        LOG() << "task_queue.size() == " << task_queue.size() << ", QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
        abort();
    } else {
        Push(std::move(*task));
    }
    return;
}

void
TaskPool::staticInitTasks(int id_unit) {
    if (THREADS_NUMBER == 1) {
        singleThreadInitTasks(id_unit);
        return;
    }
    // LOG() << "QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui* candidates_count = u_args->candidates_count;
    VertexID start_vertex = u_args->order[0];
    ui candinum_single_task = candidates_count[start_vertex] / QUEUE_CAPACITY;
    // LOG() << "candinum_single_task = " << candinum_single_task << std::endl;
    // 额外的step_remainder是第0个task来承担的
    ui num_remainder = candidates_count[start_vertex] % QUEUE_CAPACITY;
    ui candinum_first_task = candinum_single_task + num_remainder;
    // LOG() << "candinum_single_task = " << candinum_single_task << ", candinum_first_task = " << candinum_first_task << std::endl;

    // LOG() << "Init Task, start_vertex = " << start_vertex << ", candidates_count = " << candidates_count[start_vertex]
    //     << ", queue_capacity == " << QUEUE_CAPACITY << std::endl;

    ui cur_idx = 0;

for (int i = 0; i < QUEUE_CAPACITY && cur_idx < candidates_count[start_vertex]; i++) {
        if (checkOverTime()) {
            return;
        }
        // Add the remainder to the first task slot.
        ui cur_num = i == 0 ? candinum_first_task : candinum_single_task;

        // if (i > 0) continue;

        // LOG() << "cur_num: " << cur_num << ", cur_idx: " << cur_idx << std::endl;

        TaskSlot* task = new TaskSlot();
        LocalArgs*& l_args = task->l_args;
        l_args = new LocalArgs();
        l_args->id_unit = id_unit;
        l_args->allocateBuffer(TaskSlot::g_args);
        l_args->upper_depth = 0;
        l_args->cur_depth = 0;

// NOTE: 这里是专门针对DPiso的，后面要想办法分离开
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    VertexID cur_u = u_args->order[0];
    l_args->l_order[0] = u_args->order[0];
    l_args->u_idx_count[cur_u] = cur_num;
    // LOG() << "Init Task " << i << ", u_idx_count[" << cur_u << "] = " << l_args->u_idx_count[cur_u] << std::endl;

    // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
    for (ui ii = 0; ii < l_args->u_idx_count[cur_u]; ii++) {
        l_args->valid_candidate_idx[cur_u][ii] = cur_idx + ii;
    }
    setExtendable(l_args->extendable, u_args->tree, u_args->vertices_count);
} else if (USING_CANDIDATE_INDEX_FLAG) {
    int cur_depth = l_args->cur_depth;
    l_args->idx_count[cur_depth] = cur_num;
    // LOG() << "Init Task " << i << ", idx_count[" << cur_depth << "] = " << l_args->idx_count[cur_depth] << std::endl;

    // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
    for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
        l_args->valid_candidate_idx[cur_depth][ii] = cur_idx + ii;
    }
} else if (!USING_CANDIDATE_INDEX_FLAG) {
    int cur_depth = l_args->cur_depth;
    l_args->idx_count[cur_depth] = cur_num;
    // LOG() << "Init Task " << i << ", idx_count[" << cur_depth << "] = " << l_args->idx_count[cur_depth] << std::endl;

    // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
    for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
        l_args->valid_candidate[cur_depth][ii] = u_args->candidates[start_vertex][cur_idx + ii];
    }

}

        // 要先把 reserve_task_num 加上
        int tmp;
        PreReserve(false, tmp, 1);

        cur_idx += cur_num;

        if (task_queue.size() >= QUEUE_CAPACITY) {
            LOG() << "Error: TaskPool is full, init error" << std::endl;
            abort();
        } else {
            Push(std::move(*task));
        }
} // end for loop
    // LOG() << "init task done" << std::endl;
}


void
TaskPool::candidateInitTasks(int id_unit) {
    if (THREADS_NUMBER == 1) {
        singleThreadInitTasks(id_unit);
        return;
    }
    // LOG() << "QUEUE_CAPACITY == " << QUEUE_CAPACITY << std::endl;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui* candidates_count = u_args->candidates_count;
    VertexID start_vertex = u_args->order[0];
    ui candinum_single_task = candidates_count[start_vertex] / QUEUE_CAPACITY;
    // LOG() << "candinum_single_task = " << candinum_single_task << std::endl;
    // 额外的step_remainder是第0个task来承担的
    ui num_remainder = candidates_count[start_vertex] % QUEUE_CAPACITY;
    ui candinum_first_task = candinum_single_task + num_remainder;

    /**
     * static 的情况下，这个除起来会是0
     */
    if (candinum_single_task == 0) {
        candinum_single_task = 1;
        num_remainder = 0;
        candinum_first_task = 1;
    }
    // LOG() << "candinum_single_task = " << candinum_single_task << ", candinum_first_task = " << candinum_first_task << std::endl;

    // LOG() << "Init Task, start_vertex = " << start_vertex << ", candidates_count = " << candidates_count[start_vertex]
    //     << ", queue_capacity == " << QUEUE_CAPACITY << std::endl;

    ui cur_idx = 0;
for (int i = 0; i < QUEUE_CAPACITY && cur_idx < candidates_count[start_vertex]; i++) {
        if (checkOverTime()) {
            return;
        }
        // Add the remainder to the first task slot.
        ui cur_num = i == 0 ? candinum_first_task : candinum_single_task;

        // if (i > 0) continue;

        // LOG() << "cur_num: " << cur_num << ", cur_idx: " << cur_idx << std::endl;

        TaskSlot* task = new TaskSlot();
        LocalArgs*& l_args = task->l_args;
        l_args = new LocalArgs();
        l_args->id_unit = id_unit;
        l_args->allocateBuffer(TaskSlot::g_args);
        l_args->upper_depth = 0;
        l_args->cur_depth = 0;

// NOTE: 这里是专门针对DPiso的，后面要想办法分离开
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
        VertexID cur_u = u_args->order[0];
        l_args->l_order[0] = u_args->order[0];
        l_args->u_idx_count[cur_u] = cur_num;
        // LOG() << "Init Task " << i << ", u_idx_count[" << cur_u << "] = " << l_args->u_idx_count[cur_u] << std::endl;

        // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
        for (ui ii = 0; ii < l_args->u_idx_count[cur_u]; ii++) {
            l_args->valid_candidate_idx[cur_u][ii] = cur_idx + ii;
        }
        setExtendable(l_args->extendable, u_args->tree, u_args->vertices_count);
} else if (USING_CANDIDATE_INDEX_FLAG) {
        int cur_depth = l_args->cur_depth;
        l_args->idx_count[cur_depth] = cur_num;
        // LOG() << "Init Task " << i << ", idx_count[" << cur_depth << "] = " << l_args->idx_count[cur_depth] << std::endl;

        // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
        for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
            l_args->valid_candidate_idx[cur_depth][ii] = cur_idx + ii;
        }
} else if (!USING_CANDIDATE_INDEX_FLAG) {
        int cur_depth = l_args->cur_depth;
        l_args->idx_count[cur_depth] = cur_num;
        // LOG() << "Init Task " << i << ", idx_count[" << cur_depth << "] = " << l_args->idx_count[cur_depth] << std::endl;

        // 这里有错，这个valid_candidate_idx是从0到candidates_count[start_vertex]的，而不是从0到idx_count[0]
        for (ui ii = 0; ii < l_args->idx_count[cur_depth]; ii++) {
            l_args->valid_candidate[cur_depth][ii] = u_args->candidates[start_vertex][cur_idx + ii];
        }
}

if (FROZEN_SET_FLAG) {
        std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>>& FUValidCandidateIdx = l_args->FUValidCandidateIdx;
        std::unordered_map<VertexID, std::unordered_map<VertexID, ui>>& FUValidCandidatesCount = l_args->FUValidCandidatesCount;
        std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
        for (const auto& [fu, fns] : FU_fns) {
            FUValidCandidateIdx[fu] = std::unordered_map<VertexID, ui*>();
            FUValidCandidatesCount[fu] = std::unordered_map<VertexID, ui>();
            for (VertexID fn : fns) {
                FUValidCandidateIdx[fu][fn] = new ui[u_args->candidates_count[fu]];
            }
        }
}

        l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
        // LOG() << "l_limit_num == " << l_args->l_limit_num << std::endl;

        VertexID v_candi = u_args->candidates[start_vertex][cur_idx];
        l_args->upper_workload = u_args->workload_array[start_vertex][v_candi];
        // LOG() << "cur v_candi == " << v_candi << std::endl;
        // LOG() << "Init Task " << i << ", workload[" << l_args->upper_depth << "] = " << l_args->upper_workload << std::endl;
// #endif

        // 要先把 reserve_task_num 加上
        int tmp;
        PreReserve(false, tmp, 1);

        cur_idx += cur_num;

        if (task_queue.size() >= QUEUE_CAPACITY) {
            LOG() << "Error: TaskPool is full, init error" << std::endl;
            abort();
        } else {
            Push(std::move(*task));
        }
} // end for loop
    // LOG() << "init task done" << std::endl;
}

void
TaskPool::queryCandidateInitTasks() {
    LOG() << "Error: not implemented now" << std::endl;
    abort();
}

void
TaskPool::initTasks() {
    switch (TaskSlot::g_args->splitMode)
    {
        case SPLITMODE::NOSPLIT:
            singleThreadInitTasks();
            break;
        case SPLITMODE::Q:
            candidateInitTasks(0);
            break;
        case SPLITMODE::C:
            candidateInitTasks(0);
            break;
        default:
            LOG() << "Error: The split mode is not defined." << std::endl;
            abort();
            break;
    }

    return;
}

/*
    NOTE: 这里在copy的时候，不仅要split depth 对应的u的valid_candidate_idx
*/
// 还要copy，未匹配但是extendable==0的u，因为他们的valid_candidate_idx已经算出来了
void
TaskPool::splitTaskU(TaskSlot& parent_task, int down_depth) {
    auto start_time_split = std::chrono::steady_clock::now();

    // 提前返回，减少嵌套
    if (TaskSlot::g_args->splitMode == SPLITMODE::NOSPLIT || THREADS_NUMBER == 1) {
        return;
    }

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[parent_task.l_args->id_unit];

    /**
     * 对于workload方法：
     * 1. cur_depth 超过层数就不分了
     * 2. 要记录当前这个task的workload，因为在preMatch下，upper_depth只会剩下一个有效的点；
     * 首先depth==0一定一个candi，然后在第二层分完之后，第二层也一定只有一个点，此时怎么分呢；
     * 一定是分最上面的depth而不是cur_depth，因为我是回溯才分，这个逻辑有点奇怪的。所以从上往下是没问题的，只要上面剩了东西，就要拆分；
     * workload是进来的时候更新，就从upper一直遍历到cur，如果超过了阈值就拆depth，均分拆；
     * 3. 拆完之后，把新的workload算好了放进去，如果是小于阈值，就设置一个变量，提醒子任务再也不用拆分了
     * workload只统计idx+1的地方的workload，因为idx是下一个要处理的，所以是idx+1
     * 
     * 对于layer方法：
     * 就是只在前k层（cur_depth < k)拆分，而且要从upper拆起
     * 而且只要有空的就得拆出去，那个MINIMUM_TASK_LENGTH == 2好像就起了这样的效果
     */
    if (schedule_type == ScheduleType::STATIC) {
        /**
         * NOTE: 只在前k层拆分，每次拆都用layer方法
         * 暂时认为这样是写好了
        */
        if (split_type == SplitType::LAYER)
        {
            if (down_depth > PREMATCH_MAX_SPLIT_DEPTH - 1) {
                return;
            }
        } else {
            LOG() << "Error: The split type is not defined." << std::endl;
            abort();
        }
    } else if (schedule_type == ScheduleType::STATICWORKLOAD) {
        if (split_type == SplitType::LAYER) {
            if (parent_task.l_args->workload_no_split_flag) {
                return;
            }
        } else {
            LOG() << "Error: The split type is not defined." << std::endl;
            abort();
        }
    }

    if (isFull()) { return; }

    switch (schedule_type) {
        case ScheduleType::STATIC:
        {
            break;
        }

        case ScheduleType::STATICWORKLOAD:
        {
            break;
        }

        case ScheduleType::BUSY2IDLENOSTOP:
        {
            if (idle_thread_num == 0) {
                // LOG() << "No idle thread." << std::endl;
                return;
            }
            break;
        }
        /**
         * 这一块的逻辑是如果当前父亲任务的upper_depth已经到了最大的depth，那么就不再分了
         * TODO: 
         * 
         */
        case ScheduleType::BUSY2IDLEDEPTHSTOP:
        {
            if (idle_thread_num == 0) {
                return;
            }
            int tmp_depth = parent_task.l_args->upper_depth;
            int upper_left_idx_count = parent_task.l_args->idx_count[tmp_depth] - parent_task.l_args->idx[tmp_depth];
            if (parent_task.l_args->upper_depth > u_args->MAX_SPLIT_DEPTH && upper_left_idx_count <= MASSIVE_CANDIDATE_COUNT) {
                return;
            }
            break;
        }

        case ScheduleType::TIMEOUT:
        {
            if (intTimer::getClockNano() <= parent_task.start_task_time + parent_task.timeThreshold ||
                isFull()) {
                return;
            }
            break;
        }

        default:
        {
            LOG() << "Error: The schedule type is not defined." << std::endl;
            abort();
            break;
        }
    }

    int max_depth = u_args->vertices_count;
    TreeNode* tree = u_args->tree;
    auto& workload_array = u_args->workload_array;
    // for (int depth = parent_task.l_args->upper_depth; depth <= parent_task.l_args->upper_depth; ++depth) {
    for (int depth = parent_task.l_args->upper_depth; depth <= down_depth; ++depth) {
        VertexID u = parent_task.l_args->l_order[depth];
        assert(parent_task.l_args->idx[u] <= parent_task.l_args->u_idx_count[u]);
        if (parent_task.l_args->idx[u] == parent_task.l_args->u_idx_count[u]) {
            continue;
        }
        // 现在是把idx这个地方都分出去了
        int left_idx_count = parent_task.l_args->u_idx_count[u] - parent_task.l_args->idx[u];
        // LOG() << "left_idx_count = " << left_idx_count << std::endl;
        if (left_idx_count < MINIMUM_TASK_LENGTH) {
            // LOG() << "can't split Thread " << std::this_thread::get_id() << ", depth = " << depth << ", u_idx_count and idx = " << parent_task.l_args->u_idx_count[u]
            //     << ", " << parent_task.l_args->idx[depth] << std::endl;
            continue;
        }
        
        int r_count = left_idx_count / 2;
        if (r_count == 0) {
            continue;
        }
        assert(r_count > 0);
        assert(parent_task.l_args->u_idx_count[u] - r_count > 0);

        /**
         * 对于workload方法：
         * 1. cur_depth 超过层数就不分了
         * 2. 要记录当前这个task的workload，因为在preMatch下，upper_depth只会剩下一个有效的点；首先depth==0一定一个candi，然后在第二层分完之后，第二层也一定只有一个点，此时怎么分呢；
         * 一定是分最上面的depth而不是cur_depth，因为我是回溯才分，这个逻辑有点奇怪的。所以从上往下是没问题的，只要上面剩了东西，就要拆分；
         * workload是进来的时候更新，就从upper一直遍历到cur，如果超过了阈值就拆depth，均分拆；
         * 3. 拆完之后，把新的workload算好了放进去，如果是小于阈值，就设置一个变量，提醒子任务再也不用拆分了
         * workload只统计idx+1的地方的workload，因为idx是下一个要处理的，所以是idx+1
         * 
         * 对于layer方法：
         * 就是只在前k层（cur_depth < k)拆分，而且要从upper拆起
         * 而且只要有空的就得拆出去，那个MINIMUM_TASK_LENGTH == 2好像就起了这样的效果
         */
        if (schedule_type == ScheduleType::STATIC) {
            if (split_type == SplitType::LAYER) {
                // 只在前k层拆分，每次拆都用layer方法
                if (depth > PREMATCH_MAX_SPLIT_DEPTH - 1) {
                    return;
                }
            } else {
                LOG() << "Error: The split type is not defined." << std::endl;
                abort();
            }
        }
        else if (schedule_type == ScheduleType::STATICWORKLOAD) {
            if (split_type == SplitType::LAYER) {

                /**
                 * 这里是第一层可以分的，要从upper开始算local_workload。
                 * 
                 */
                if (parent_task.l_args->upper_workload <= u_args->total_workload / THREADS_NUMBER) {
                    parent_task.l_args->workload_no_split_flag = true;
                    return;
                }
            } else {
                LOG() << "Error: The split type is not defined." << std::endl;
                abort();
            }
        }

        /*
            NOTE: 在这里要去 PreReserve, 如果失败了要直接返回
                    成功了就正常分配，逻辑不变
        */
        int containerCount;
        if (!PreReserve(split_type == SplitType::LAYER, containerCount, left_idx_count)) {
            return;
        }

        /**
         * NOTE: valid_candidate_idx 和 valid_candidate 要彻底分开来，二者的workload_array的获取是不一样的
        */

        // LOG() << "Parent's split_depth = " << depth << ", u == " << u
        //     << ", u_idx_count and idx = " << parent_task.l_args->u_idx_count[u] << ", " << parent_task.l_args->idx[u]
        //     << ", r_count = " << r_count << std::endl;

        // std::ostringstream oss;
        // oss << "Before Parent Task valid_candidate_idx or valid_candidate :";
        // for (int i = 0; i < parent_task.l_args->u_idx_count[u]; ++i) {
        //     oss << parent_task.l_args->valid_candidate_idx[u][i] << " ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        // 拆分完毕，还是把新的产生的calculated_valid_candidate_idx给拼回到parent里面吧
        std::vector<ui*> calculated_valid_candidate_idx(containerCount, nullptr);
        for (int ii = 0; ii < containerCount; ii++) {
            calculated_valid_candidate_idx[ii] = new ui[parent_task.l_args->u_idx_count[u]];
        }
        std::vector<ui> idx_count_cur_vec(containerCount, 0);

        ui* left_valid_candidate_idx = parent_task.l_args->valid_candidate_idx[u] + parent_task.l_args->idx[u];

        // oss << "left_valid_candidate_idx : ";
        // for (int i = 0; i < left_idx_count; ++i) {
        //     oss << left_valid_candidate_idx[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        // 根据不同的分配策略进行任务划分
        if (split_type == SplitType::LINEAR)
        {
            linearDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }
        else if (split_type == SplitType::UPPERONE)
        {
            upperoneDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }
        else if (split_type == SplitType::WORKLOAD)
        {
            workloadDistribution(parent_task, containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec, u_args, u);
        }
        else if (split_type == SplitType::LAYER)
        {
            layerDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }

        r_count = left_idx_count - idx_count_cur_vec[0];

        // LOG() << "Log all the calculated_valid_candidate_idx and idx_count_cur_vec : " << std::endl;
        // oss.str("");
        // oss << "containerCount == " << containerCount << std::endl;
        // for (int ii = 0; ii < containerCount; ii++) {
        //     oss << "calculated_valid_candidate_idx[" << ii << "] : ";
        //     for (int i = 0; i < idx_count_cur_vec[ii]; ++i) {
        //         oss << calculated_valid_candidate_idx[ii][i] << ", ";
        //     }
        //     oss << std::endl;
        // }
        // LOG() << oss.str();
        // oss.str("");

        int64_t cur_layer_workload = 0;
        if (schedule_type == ScheduleType::STATICWORKLOAD) {
            for (int ii = 0; ii < parent_task.l_args->u_idx_count[u]; ++ii) {
                VertexID candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][parent_task.l_args->valid_candidate_idx[u][ii]]
                                                            : parent_task.l_args->valid_candidate[u][ii];
                // LOG() << "candi == " << candi << ", workload_array[u][candi] == " << workload_array[u][candi] << std::endl;                                        
                cur_layer_workload += workload_array[u][candi];
            }
        }
        

if (USING_CANDIDATE_INDEX_FLAG) {
        std::memcpy(
            parent_task.l_args->valid_candidate_idx[u] + parent_task.l_args->idx[u],
            calculated_valid_candidate_idx[0],
            idx_count_cur_vec[0] * sizeof(ui)
        );
} else {
        std::memcpy(
            parent_task.l_args->valid_candidate[u] + parent_task.l_args->idx[u],
            calculated_valid_candidate_idx[0],
            idx_count_cur_vec[0] * sizeof(ui)
        );
}
        parent_task.l_args->u_idx_count[u] -= r_count;
        parent_task.l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
        // LOG() << "parent_task.l_limit_num == " << parent_task.l_args->l_limit_num << std::endl;

        if (schedule_type == ScheduleType::STATICWORKLOAD) {
            // for (int ii = 0; ii < parent_task.l_args->idx_count[depth]; ++ii) {
            //     VertexID candi = USING_CANDIDATE_INDEX_FLAG ? parent_task.l_args->valid_candidate_idx[depth][ii]
            //                                                 : parent_task.l_args->valid_candidate[depth][ii];
            //     LOG() << "candi == " << candi << ", workload_array[u][candi] == " << workload_array[u][candi] << std::endl;                                        
            //     cur_layer_workload += workload_array[u][candi];
            // }
            VertexID cur_candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][calculated_valid_candidate_idx[0][0]]
                                                            : calculated_valid_candidate_idx[0][0];
                                                            
            double_t child_upper_workload = double_t(workload_array[u][cur_candi]) / cur_layer_workload * parent_task.l_args->upper_workload;
            // LOG() << "workload_array[u][cur_candi] == " << workload_array[u][cur_candi] << std::endl;
            // LOG() << "cur_layer_workload == " << cur_layer_workload << std::endl;
            // LOG() << "parent_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            // LOG() << "child_upper_workload == " << child_upper_workload << std::endl;
            parent_task.l_args->upper_workload = child_upper_workload;
            // LOG() << "unit total_workload == " << u_args->total_workload << std::endl;
            // LOG() << "New parent_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            if (child_upper_workload < u_args->total_workload / THREADS_NUMBER) {
                parent_task.l_args->workload_no_split_flag = true;
            }
        }

for (int ii = 1; ii < containerCount; ++ii) { // start for loop
        if (idx_count_cur_vec[ii] == 0) {
            continue;
        }

        // [i]是新任务的，老的还要用[0]赋值
        TaskSlot* task = new TaskSlot();
        LocalArgs*& l_args = task->l_args;

        l_args = new LocalArgs(
            TaskSlot::g_args,
            *parent_task.l_args,
            // parent_task.l_args->u_idx_count[u] - r_count,
            // parent_task.l_args->u_idx_count[u],
            depth,
            down_depth,
            calculated_valid_candidate_idx[ii],
            idx_count_cur_vec[ii]
        );
        l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
        // LOG() << "l_limit_num == " << l_args->l_limit_num << std::endl;

        /**
         * 新加的static-workload方法，默认是从上到下分的。
         * 
         */
        if (schedule_type == ScheduleType::STATICWORKLOAD) {

            VertexID cur_candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][calculated_valid_candidate_idx[ii][0]]
                                                            : calculated_valid_candidate_idx[ii][0];

            double_t child_upper_workload = double_t(workload_array[u][cur_candi]) / cur_layer_workload * parent_task.l_args->upper_workload;
            l_args->upper_workload = child_upper_workload;
            // LOG() << "unit total_workload == " << u_args->total_workload << std::endl;
            // LOG() << "New child_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            if (child_upper_workload < u_args->total_workload / THREADS_NUMBER) {
                l_args->workload_no_split_flag = true;
            }
        }        

        // 打印 l_args->valid_candidate_idx 或 valid_candidate
        
        // oss << "New Task " << ii << "'s valid_candidate_idx or valid_candidate after Distribution:";
        // for (int i = 0; i < idx_count_cur_vec[ii]; ++i) {
        //     oss << calculated_valid_candidate_idx[ii][i] << " ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        // LOG() << "QUEUE_CAPACITY : " << QUEUE_CAPACITY << std::endl;

        /*
            下面这一大段
                (1) 是复制 extendable == 0 的 u 的 valid_candidate_idx 到 child_task 里
                (2) 设置 child_task 的 extendable，全部设为初始值，再挨个减
                
        */
        std::unordered_set<VertexID> matched;
        for (int i = 0; i < depth; i++) {
            VertexID mapped_u = parent_task.l_args->l_order[i];
            matched.insert(mapped_u);
        }

        for (VertexID tmpu = 0; tmpu < max_depth; tmpu++) {
            if (matched.find(tmpu) != matched.end() || tmpu == u) {
                continue;
            }
            if (tmpu == u) {
                continue;
            }
            // assert(parent_task.l_args->extendable[tmpu] >= 0);
            if (parent_task.l_args->extendable[tmpu] == 0) {
                // 
                std::memcpy(
                    l_args->valid_candidate_idx[tmpu],
                    parent_task.l_args->valid_candidate_idx[tmpu],
                    l_args->u_idx_count[tmpu] * sizeof(ui)
                );
            }
        }

        /*
            1. 要设置一下extendable，和父亲的有区别的，从upper到down的extendabel全部恢复
            2. 这些部分要做updateExtendable的操作，因为这些是已经匹配过的东西了
            3. 好像也要做vec_rank_queue的操作，因为可能这一层有些点已经是没有bn的了，在这里是要复制vec_rank_queue的吧，至少要depth这一层
                所以init的时候也要做这个初始化操作，好烦，又要放到l_args里了吗，夸张啊，这里加个#define吧，太冗余了
                直接复制整体的vec_rank_queue好了，反正上层的也不会back到，就按最简单的来
        */
        setExtendable(l_args->extendable, tree, u_args->vertices_count);
        for (int i = 0; i < depth; i++) {
            VertexID mapped_u = parent_task.l_args->l_order[i];
            TreeNode &node = tree[mapped_u];
            for (ui j = 0; j < node.fn_count_; ++j) {
                VertexID tmpu = node.fn_[j];
                l_args->extendable[tmpu] -= 1;
            }
        }
        // 这里复制 vec_rank_queue 和 splited_u_set
        for (int i = 0; i < depth; i++) {
            l_args->vec_rank_queue.push_back(parent_task.l_args->vec_rank_queue[i]);
        }
        parent_task.l_args->splited_u_set.emplace(u);
        l_args->splited_u_set = parent_task.l_args->splited_u_set;

        // push 到任务队列中
        Push(std::move(*task));
        cv.notify_one();
} // end for loop

        parent_task.start_task_time = intTimer::getClockNano();
        break;
    }

    return handleSplitTime(start_time_split);
}

// split parent_task to 2 tasks
// NOTE: logic error when simulate single thread algo, for size() is 0 when single thread is dealing with the only task, and it will split the task
// so size() >= QUEUE_CAPACITY - 1 is what we need
// 考虑把single thread的情况完全分开来，所以这里需要 size() >= QUEUE_CAPACITY - 1 还是 Size() >= QUEUE_CAPACITY呢
// NOTE: 现在single thread已经完全分开来了
void
TaskPool::splitTask(TaskSlot& parent_task, int down_depth) {
    auto start_time_split = std::chrono::steady_clock::now();
    // 提前返回，减少嵌套
    if (TaskSlot::g_args->splitMode == SPLITMODE::NOSPLIT || THREADS_NUMBER == 1) {
        return;
    }

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[parent_task.l_args->id_unit];
    auto& workload_array = u_args->workload_array;

    /**
     * 对于workload方法：
     * 1. cur_depth 超过层数就不分了
     * 2. 要记录当前这个task的workload，因为在preMatch下，upper_depth只会剩下一个有效的点；
     * 首先depth==0一定一个candi，然后在第二层分完之后，第二层也一定只有一个点，此时怎么分呢；
     * 一定是分最上面的depth而不是cur_depth，因为我是回溯才分，这个逻辑有点奇怪的。所以从上往下是没问题的，只要上面剩了东西，就要拆分；
     * workload是进来的时候更新，就从upper一直遍历到cur，如果超过了阈值就拆depth，均分拆；
     * 3. 拆完之后，把新的workload算好了放进去，如果是小于阈值，就设置一个变量，提醒子任务再也不用拆分了
     * workload只统计idx+1的地方的workload，因为idx是下一个要处理的，所以是idx+1
     * 
     * 对于layer方法：
     * 就是只在前k层（cur_depth < k)拆分，而且要从upper拆起
     * 而且只要有空的就得拆出去，那个MINIMUM_TASK_LENGTH == 2好像就起了这样的效果
     */
    if (schedule_type == ScheduleType::STATIC) {
        /**
         * NOTE: 只在前k层拆分，每次拆都用layer方法
         * 暂时认为这样是写好了
        */
        if (split_type == SplitType::LAYER)
        {
            if (down_depth > PREMATCH_MAX_SPLIT_DEPTH - 1) {
                return;
            }
        } else {
            LOG() << "Error: The split type is not defined." << std::endl;
            abort();
        }
    } else if (schedule_type == ScheduleType::STATICWORKLOAD) {
        if (split_type == SplitType::LAYER) {
            if (parent_task.l_args->workload_no_split_flag) {
                return;
            }
        } else {
            LOG() << "Error: The split type is not defined." << std::endl;
            abort();
        }
    }

    if (isFull()) { return;}

    switch (schedule_type) {
        case ScheduleType::STATIC:
        {
            break;
        }

        case ScheduleType::STATICWORKLOAD:
        {
            break;
        }

        case ScheduleType::BUSY2IDLENOSTOP:
        {
            if (idle_thread_num == 0) {
                // LOG() << "No idle thread." << std::endl;
                return;
            }
            break;
        }
        
        case ScheduleType::BUSY2IDLEDEPTHSTOP:
        {
            if (idle_thread_num == 0) {
                return;
            }
            int tmp_depth = parent_task.l_args->upper_depth;
            int upper_left_idx_count = parent_task.l_args->idx_count[tmp_depth] - parent_task.l_args->idx[tmp_depth];
            if (parent_task.l_args->upper_depth > u_args->MAX_SPLIT_DEPTH && upper_left_idx_count <= MASSIVE_CANDIDATE_COUNT) {
                return;
            }

            break;
        }

        case ScheduleType::TIMEOUT:
        {
            if (intTimer::getClockNano() <= parent_task.start_task_time + parent_task.timeThreshold ||
                isFull()) {
                return;
            }
            break;
        }

        default:
        {
            LOG() << "Error: The schedule type is not defined." << std::endl;
            abort();
            break;
        }
    }

    // from upper_depth, try to split parent_task
    // NOTE: the element at idx[0] is assumed to be used here, but not actually
    /*
        这里的逻辑要改掉，因为只有到了他这一层，他才知道有多少idx_count
        LFTJ是每匹配一个就求交一次，动态缩小idx_count，但是其他方法不行
        所以统一改为只有upper能？？
    */ 

    // LOG() << "Q.size == " << Size() << ", Start split" << std::endl;

    for (int depth = parent_task.l_args->upper_depth; depth <= down_depth; ++depth) {
        VertexID u = u_args->order[depth];
        int left_idx_count = parent_task.l_args->idx_count[depth] - parent_task.l_args->idx[depth];

        if (left_idx_count < MINIMUM_TASK_LENGTH) 
        {
            continue;
        }
        int r_count = left_idx_count / 2;
        if (r_count <= 0) {
            continue;
        }

        /**
         * 对于workload方法：
         * 1. cur_depth 超过层数就不分了
         * 2. 要记录当前这个task的workload，因为在preMatch下，upper_depth只会剩下一个有效的点；首先depth==0一定一个candi，然后在第二层分完之后，第二层也一定只有一个点，此时怎么分呢；
         * 一定是分最上面的depth而不是cur_depth，因为我是回溯才分，这个逻辑有点奇怪的。所以从上往下是没问题的，只要上面剩了东西，就要拆分；
         * workload是进来的时候更新，就从upper一直遍历到cur，如果超过了阈值就拆depth，均分拆；
         * 3. 拆完之后，把新的workload算好了放进去，如果是小于阈值，就设置一个变量，提醒子任务再也不用拆分了
         * workload只统计idx+1的地方的workload，因为idx是下一个要处理的，所以是idx+1
         * 
         * 对于layer方法：
         * 就是只在前k层（cur_depth < k)拆分，而且要从upper拆起
         * 而且只要有空的就得拆出去，那个MINIMUM_TASK_LENGTH == 2好像就起了这样的效果
        */

        if (schedule_type == ScheduleType::STATIC) {
            if (split_type == SplitType::LAYER) {
                // 只在前k层拆分，每次拆都用layer方法
                if (depth > PREMATCH_MAX_SPLIT_DEPTH - 1) {
                    return;
                }
            } else {
                LOG() << "Error: The schedule type STATIC must be with LAYER split type." << std::endl;
                abort();
            }
        }
        else if (schedule_type == ScheduleType::STATICWORKLOAD) {
            if (split_type == SplitType::LAYER) {

                // LOG() << "unit total_workload == " << u_args->total_workload << std::endl;
                // LOG() << "parent_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;

                if (parent_task.l_args->upper_workload <= u_args->total_workload / THREADS_NUMBER) {
                    parent_task.l_args->workload_no_split_flag = true;
                    return;
                }
            } else {
                LOG() << "Error: The schedule type STATICWORKLOAD must be with LAYER split type." << std::endl;
                LOG() << "split_type = " << split_type << std::endl;
                abort();
            }
        }

        /*
            NOTE: 在这里要去 preserve，如果失败了要直接返回
                    成功了就正常分配，逻辑不变
        */
        int containerCount;
        if (!PreReserve(split_type == SplitType::LAYER, containerCount, left_idx_count)) {
            return;
        }

        /**
         * NOTE: valid_candidate_idx 和 valid_candidate 要彻底分开来，二者的workload_array的获取是不一样的
         */

        // 拆分完毕，还是把新的产生的calculated_valid_candidate_idx给拼回到parent里面吧
        std::vector<ui*> calculated_valid_candidate_idx(containerCount, nullptr);
        for (int ii = 0; ii < containerCount; ii++) {
            calculated_valid_candidate_idx[ii] = new ui[parent_task.l_args->idx_count[depth]];
        }
        std::vector<ui> idx_count_cur_vec(containerCount, 0);

        ui* left_valid_candidate_idx;
if (USING_CANDIDATE_INDEX_FLAG) {
       left_valid_candidate_idx = parent_task.l_args->valid_candidate_idx[depth] + parent_task.l_args->idx[depth];
} else {
       left_valid_candidate_idx = parent_task.l_args->valid_candidate[depth] + parent_task.l_args->idx[depth];
}

        // LOG() << "Parent's split_depth = " << depth << ", u == " << u
        // << ", idx_count and idx = " << parent_task.l_args->idx_count[depth] << ", " << parent_task.l_args->idx[depth]
        // << ", r_count = " << r_count << std::endl;

        // std::ostringstream oss;
        // oss << "Before Parent Task valid_candidate_idx or valid_candidate :";
        // for (int i = 0; i < parent_task.l_args->idx_count[depth]; ++i) {
        // oss << parent_task.l_args->valid_candidate_idx[depth][i] << " ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        // 根据不同的分配策略进行任务划分
        if (split_type == SplitType::LINEAR)
        {
            linearDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }
        else if (split_type == SplitType::UPPERONE)
        {
            upperoneDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }
        else if (split_type == SplitType::WORKLOAD)
        {
            workloadDistribution(parent_task, containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec, u_args, u);
        } else if (split_type == SplitType::LAYER) {
            // LOG() << "left_capacity == " << containerCount << std::endl;
            layerDistribution(containerCount, left_valid_candidate_idx, left_idx_count,
                calculated_valid_candidate_idx, idx_count_cur_vec);
        }

        r_count = left_idx_count - idx_count_cur_vec[0];


        // LOG() << "r_count == " << r_count << std::endl;

        // LOG() << "Log all the calculated_valid_candidate_idx and idx_count_cur_vec : " << std::endl;
        // oss.str("");
        // oss << "containerCount == " << containerCount << std::endl;
        // for (int ii = 0; ii < containerCount; ii++) {
        //     oss << "calculated_valid_candidate_idx[" << ii << "] : ";
        //     for (int i = 0; i < idx_count_cur_vec[ii]; ++i) {
        //         oss << calculated_valid_candidate_idx[ii][i] << ", ";
        //     }
        //     oss << std::endl;
        // }
        // LOG() << oss.str();
        // oss.str("");

        int64_t cur_layer_workload = 0;
        if (schedule_type == ScheduleType::STATICWORKLOAD) {
            for (int ii = 0; ii < parent_task.l_args->idx_count[depth]; ++ii) {
                VertexID candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][parent_task.l_args->valid_candidate_idx[depth][ii]]
                                                            : parent_task.l_args->valid_candidate[depth][ii];
                // LOG() << "candi == " << candi << ", workload_array[u][candi] == " << workload_array[u][candi] << std::endl;                                        
                cur_layer_workload += workload_array[u][candi];
            }
        }
        

if (USING_CANDIDATE_INDEX_FLAG) {
        std::memcpy(
            parent_task.l_args->valid_candidate_idx[depth] + parent_task.l_args->idx[depth],
            calculated_valid_candidate_idx[0],
            idx_count_cur_vec[0] * sizeof(ui)
        );
} else {
        std::memcpy(
            parent_task.l_args->valid_candidate[depth] + parent_task.l_args->idx[depth],
            calculated_valid_candidate_idx[0],
            idx_count_cur_vec[0] * sizeof(ui)
        );
}
        parent_task.l_args->idx_count[depth] -= r_count;
        parent_task.l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
        // LOG() << "parent_task.l_limit_num == " << parent_task.l_args->l_limit_num << std::endl;

        // 打印 parent_task.l_args->valid_candidate_idx 或 valid_candidate
        // std::ostringstream oss;
        // oss << "After Distribution Parent Task valid_candidate_idx or valid_candidate :";
        // for (int i = 0; i < parent_task.l_args->idx_count[depth]; ++i) {
        //     oss << parent_task.l_args->valid_candidate_idx[depth][i] << " ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        if (schedule_type == ScheduleType::STATICWORKLOAD) {
            // for (int ii = 0; ii < parent_task.l_args->idx_count[depth]; ++ii) {
            //     VertexID candi = USING_CANDIDATE_INDEX_FLAG ? parent_task.l_args->valid_candidate_idx[depth][ii]
            //                                                 : parent_task.l_args->valid_candidate[depth][ii];
            //     LOG() << "candi == " << candi << ", workload_array[u][candi] == " << workload_array[u][candi] << std::endl;                                        
            //     cur_layer_workload += workload_array[u][candi];
            // }
            VertexID cur_candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][calculated_valid_candidate_idx[0][0]]
                                                            : calculated_valid_candidate_idx[0][0];
                                                            
            double_t child_upper_workload = double_t(workload_array[u][cur_candi]) / cur_layer_workload * parent_task.l_args->upper_workload;
            // LOG() << "workload_array[u][cur_candi] == " << workload_array[u][cur_candi] << std::endl;
            // LOG() << "cur_layer_workload == " << cur_layer_workload << std::endl;
            // LOG() << "parent_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            // LOG() << "child_upper_workload == " << child_upper_workload << std::endl;
            parent_task.l_args->upper_workload = child_upper_workload;
            // LOG() << "unit total_workload == " << u_args->total_workload << std::endl;
            // LOG() << "New parent_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            if (child_upper_workload < u_args->total_workload / THREADS_NUMBER) {
                parent_task.l_args->workload_no_split_flag = true;
            }
        }

for (int ii = 1; ii < containerCount; ++ii) { // start for loop
        if (idx_count_cur_vec[ii] == 0) {
            continue;
        }

        TaskSlot* task = new TaskSlot();
        LocalArgs*& l_args = task->l_args;
        // TODO: [i]是新任务的，老的还要用[0]赋值
        l_args = new LocalArgs(
            TaskSlot::g_args,
            *parent_task.l_args,
            // parent_task.l_args->u_idx_count[u] - r_count,
            // parent_task.l_args->u_idx_count[u],
            depth,
            down_depth,
            calculated_valid_candidate_idx[ii],
            idx_count_cur_vec[ii]
        );

if (FROZEN_SET_FLAG) {
        // === ✅ 初始化阶段 ===
        auto& dstIdx = l_args->FUValidCandidateIdx;
        auto& dstCount = l_args->FUValidCandidatesCount;
        auto& FU_fns = u_args->FU_fns;

        for (const auto& [fu, fns] : FU_fns) {
            dstIdx[fu] = std::unordered_map<VertexID, ui*>();
            dstCount[fu] = std::unordered_map<VertexID, ui>();
            for (VertexID fn : fns) {
                // 先分配内存，这里用 candidates_count[fu] 作为最大长度
                dstIdx[fu][fn] = new ui[u_args->candidates_count[fu]];
            }
        }

        // 拷贝内容
        const auto& srcIdx = parent_task.l_args->FUValidCandidateIdx;
        const auto& srcCount = parent_task.l_args->FUValidCandidatesCount;

        for (const auto& [u, inner_map] : srcCount) {
            for (const auto& [v, count] : inner_map) {
                if (srcIdx.count(u) == 0 || srcIdx.at(u).count(v) == 0 || srcIdx.at(u).at(v) == nullptr)
                    continue;

                const ui* src_arr = srcIdx.at(u).at(v);
                ui* dst_arr = dstIdx.at(u).at(v);
                std::copy(src_arr, src_arr + count, dst_arr);
                dstCount[u][v] = count;
            }
        }
}

        l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
        // LOG() << "l_limit_num == " << l_args->l_limit_num << std::endl;

        /**
         * 新加的static-workload方法，默认是从上到下分的。
         * 
         */
        if (schedule_type == ScheduleType::STATICWORKLOAD) {

            VertexID cur_candi = USING_CANDIDATE_INDEX_FLAG ? u_args->candidates[u][calculated_valid_candidate_idx[ii][0]]
                                                            : calculated_valid_candidate_idx[ii][0];

            double_t child_upper_workload = double_t(workload_array[u][cur_candi]) / cur_layer_workload * parent_task.l_args->upper_workload;
            l_args->upper_workload = child_upper_workload;
            // LOG() << "unit total_workload == " << u_args->total_workload << std::endl;
            // LOG() << "New child_task.l_args->upper_workload == " << parent_task.l_args->upper_workload << std::endl;
            if (child_upper_workload < u_args->total_workload / THREADS_NUMBER) {
                l_args->workload_no_split_flag = true;
            }
        }

        // // 打印 l_args->valid_candidate_idx 或 valid_candidate
        // oss.str("");
        // oss << "New Task " << ii << "'s valid_candidate_idx or valid_candidate after Distribution:";
        // for (int jj = 0; jj < idx_count_cur_vec[ii]; ++jj) {
        //     oss << calculated_valid_candidate_idx[ii][jj] << " ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();

        // LOG() << "QUEUE_CAPACITY : " << QUEUE_CAPACITY << std::endl;

        Push(std::move(*task));
        // LOG() << "notify one" << std::endl;
        cv.notify_one();

} // end for loop

        parent_task.start_task_time = intTimer::getClockNano();
        break;
    }

    return handleSplitTime(start_time_split);
}

void
TaskPool::handleSplitTime(std::chrono::_V2::steady_clock::time_point start_time_split) {
    auto elapsed_time_split = std::chrono::steady_clock::now() - start_time_split;
    TaskSlot::g_args->addTimeSplit(
        std::this_thread::get_id(), std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_time_split).count()
    );
}
/*
    这个 containerCount 比较特殊，别人传进来都是2，他是 queue 的 Size() - CAPACITY
    并且在这个分完之后，上面的处理逻辑也不一样，得新建 containerCount - 1 个新的任务
    TODO : 需要验证 Size() - CAPACITY 是不是符合预期，每种setting都要打印出来看一下，不能稀里糊涂过去跑实验。
*/
void
TaskPool::layerDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count, 
                            std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec) {

    // 分配元素到 containerCount 中
    ui cur_start = 0;
    
    // 当 containerCount > left_idx_count 时，确保 idx_count_cur_vec[0] 设为 1
    if (containerCount > left_idx_count) {
        idx_count_cur_vec[0] = 1;
    } else {
        idx_count_cur_vec[0] = left_idx_count - (containerCount - 1);
    }
    
    std::copy_n(left_valid_candidate_idx + cur_start, idx_count_cur_vec[0], calculated_valid_candidate_idx_vec[0]);
    cur_start += idx_count_cur_vec[0];

    // 然后将剩余的每个元素分配给其他容器
    for (int i = 1; i < containerCount; i++) {
        if (cur_start < left_idx_count) {
            idx_count_cur_vec[i] = 1;  // 每个后面的 container 只分配一个元素
            std::copy_n(left_valid_candidate_idx + cur_start, 1, calculated_valid_candidate_idx_vec[i]);
            cur_start++;
        } else {
            idx_count_cur_vec[i] = 0;  // 如果没有更多的元素，分配 0
        }
    }

    // printDistribution(containerCount, calculated_valid_candidate_idx_vec, idx_count_cur_vec);
}

void
TaskPool::linearDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
                            std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec) {

// 均分left_valid_candidate_idx到containerCount个container中
// 用std::copy_n
    ui each_num = left_idx_count / containerCount;
    ui num_remainder = left_idx_count % containerCount;
    ui cur_start = 0;
    for (int i = 0; i < containerCount; i++) {
        ui cur_num = i == 0 ? each_num + num_remainder : each_num;
        idx_count_cur_vec[i] = cur_num;
        std::copy_n(left_valid_candidate_idx + cur_start, cur_num, calculated_valid_candidate_idx_vec[i]);
        cur_start += cur_num;
    }

    // printDistribution(containerCount, calculated_valid_candidate_idx_vec, idx_count_cur_vec);
}

void
TaskPool::workloadDistribution(TaskSlot& parent_task, int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
    std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec, UnitArgs* u_args, VertexID u)
{

    /*
        先写一个valid_candidate_idx的版本，后面再加一个valid_candidates的版本
    */
    auto& workload_array = u_args->workload_array;
    ui** candidates = u_args->candidates;

    // 任务队列 (workload, vertex), 大根堆
    std::priority_queue<std::pair<int, VertexID>> H_plus;
    // 负载队列 (workload, index of container)，小根堆
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> H_w;

    // 初始化 H_plus
for (int i = left_idx_count - 1; i >= 0; --i) {
        VertexID v;
if (USING_CANDIDATE_INDEX_FLAG) {
        int idx = left_valid_candidate_idx[i];
        v = candidates[u][left_valid_candidate_idx[i]];
        int workload = workload_array[u][v];
        H_plus.emplace(workload, idx);
} else {
        v = left_valid_candidate_idx[i];
        int workload = workload_array[u][v];
        H_plus.emplace(workload, v);
}
}

    // 初始化 H_w，先添加空任务
    for (int i = 0; i < containerCount; ++i) {
        H_w.emplace(0, i); // 负载初始化为 0，任务编号为 i
    }

    // 任务分配
    while (!H_plus.empty()) {
        // 取出 H_plus 队列中的任务 (m+)
        auto [m_plus_load, m_plus] = H_plus.top();
        H_plus.pop();

        // 取出 H_w 队列中的任务 (w)
        auto [w_load, index] = H_w.top();
        H_w.pop();

        // 更新 w 的任务集合 M+
        // LOG() << "add vertex(idx) " << m_plus << " to container " << index << std::endl;
        calculated_valid_candidate_idx_vec[index][idx_count_cur_vec[index]++] = m_plus;

        // 更新 w 的负载
        w_load += m_plus_load;

        // 重新加入 H_w 进行后续处理
        H_w.emplace(w_load, index);
    }

    // std::ostringstream oss;
    // oss << "Output H_w:" << std::endl;
    // while (!H_w.empty()) {
    //     auto [w_load, index] = H_w.top();
    //     H_w.pop();
    //     oss << "Container " << index << " workload = " << w_load << std::endl;
    // }
    // LOG() << oss.str();

    // printDistribution(containerCount, calculated_valid_candidate_idx_vec, idx_count_cur_vec);
}

void
TaskPool::upperoneDistribution(int containerCount, ui* left_valid_candidate_idx, ui left_idx_count,
    std::vector<ui*> calculated_valid_candidate_idx_vec, std::vector<ui>& idx_count_cur_vec)
{
    // 确保至少有一个元素
    if (left_idx_count == 0) {
        abort();
        return;
    }

    if (left_idx_count == 1) {
        // 只有一个元素，分配给 calculated_valid_candidate_idx_vec[1]
        calculated_valid_candidate_idx_vec[1][0] = left_valid_candidate_idx[0];
        idx_count_cur_vec[1] = 1;
        idx_count_cur_vec[0] = 0;
    } else {
        // 多个元素时，前面的元素分给 calculated_valid_candidate_idx_vec[0]，最后一个元素分给 calculated_valid_candidate_idx_vec[1]
        ui count_0 = left_idx_count - 1;
        ui count_1 = 1;

        std::copy_n(left_valid_candidate_idx, count_0, calculated_valid_candidate_idx_vec[0]);
        calculated_valid_candidate_idx_vec[1][0] = left_valid_candidate_idx[left_idx_count - 1];

        idx_count_cur_vec[0] = count_0;
        idx_count_cur_vec[1] = count_1;
    }

    // printDistribution(containerCount, calculated_valid_candidate_idx_vec, idx_count_cur_vec);
}

void
TaskPool::printDistribution(int containerCount,
    std::vector<ui*> calculated_valid_candidate_idx_vec,
    std::vector<ui> idx_count_cur_vec)
{
    // 打印任务分配情况
    std::stringstream oss;
    oss << "========== Workload Distribution Result ==========" << std::endl;
    
    // 遍历每个 container
    for (int i = 0; i < containerCount; ++i) {
        oss << "Container " << i << " (idx_count_cur_vec = " << idx_count_cur_vec[i] << ") : ";
        for (ui j = 0; j < idx_count_cur_vec[i]; ++j) {
            oss << calculated_valid_candidate_idx_vec[i][j] << " ";
        }
        oss << std::endl;
    }
    LOG() << oss.str();
}
