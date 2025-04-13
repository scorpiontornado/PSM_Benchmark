#ifndef ARGS_H
#define ARGS_H

#include "graph/graph.h"
#include "utility/splitGraph/SplitGraph.h"
#include "utility/relations/partialMatch.h"

#define NANOSECTOSEC(elapsed_time) ((elapsed_time)/(double)1000000000)
#define BYTESTOMB(memory_cost) ((memory_cost)/(double)(1024 * 1024))

class EvaluateQuery;
class GLOBALARGS;

// Min priority queue.
static const auto extendable_vertex_compare = [](std::pair<std::pair<VertexID, ui>, ui> l, std::pair<std::pair<VertexID, ui>, ui> r) {
    if (l.first.second == 1 && r.first.second != 1) {
        return true;
    }
    else if (l.first.second != 1 && r.first.second == 1) {
        return false;
    }
    else
    {
        return l.second > r.second;
    }
};
typedef std::priority_queue<std::pair<std::pair<VertexID, ui>, ui>, std::vector<std::pair<std::pair<VertexID, ui>, ui>>,
        decltype(extendable_vertex_compare)> dpiso_min_pq;

class UnitArgs {
  public:
    UnitArgs() = default;

    UnitArgs(InducedSubgraph* query_graph, size_t output_limit_num);

    ~UnitArgs();

    void mapUidCandidates(GLOBALARGS* g_args);
    void mapUidEdgeMatrix(GLOBALARGS* g_args);

    void addPartialMatch(ui* embedding);

    void generateCircinusLayer();

    void printCandidates();

    void printEdgeMatrix();

    void printTECandidates();

    // 保存了unit graph的类
    const InducedSubgraph* query_graph{nullptr};
    Edges ***edge_matrix{nullptr};
    // 只在CECI才复制
    std::vector<std::unordered_map<VertexID, std::vector<VertexID >>> TE_Candidates;
    std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> NTE_Candidates;
    ui **candidates{nullptr};
    ui *candidates_count{nullptr};
    TreeNode* tree{nullptr};
    ui *tree_order{nullptr}; // 这个是bfs_order
    ui *order{nullptr}; // 这个是匹配order
// #ifdef ENABLE_FROZEN_SET
    /*
        1. 计算一个vertex cover
        2. 根据 order 和 vertex cover，计算需要冻结的点，以及他们的有序的 fns
    */
    // none vertex cover
    std::unordered_set<VertexID> FUSet;
    std::unordered_set<VertexID> vertexCover;
    std::unordered_set<std::pair<VertexID, VertexID>, PairHash> edgesCovered; // 用于记录已覆盖的边
    // key: frozen_u, value: forward u's of u
    std::unordered_map<VertexID, std::vector<VertexID>> FU_fns;
    // key1 : u \in nfu, key2 : fu that is bn of u, value : pre vertex
    std::unordered_map<VertexID, std::unordered_map<VertexID, VertexID>> preu;
    ui *u_depth{nullptr};
// #endif

    ui* pivots{nullptr};
    ui **bn{nullptr};
    ui *bn_count{nullptr};
    ui **weight_array{nullptr};
    
    std::vector<std::unordered_map<VertexID, int64_t>> workload_array;
    double_t total_workload{0.0};
    int MAX_SPLIT_DEPTH{0};

    ui output_limit{0};
    ui vertices_count{0};

    PartialMatch* unit_partial_match{nullptr};

    bool needCandidates{false};
    bool needEdgeMatrix{false};

  private:
    std::mutex pm_mtx;
};

struct GLOBALARGS {
    GLOBALARGS(const Graph *data_graph, const Graph *query_graph,
        Edges ***edge_matrix, ui **candidates, ui *candidates_count,
        size_t output_limit_num, int64_t time_limit);

    // 禁止拷贝构造函数
    explicit GLOBALARGS(const GLOBALARGS& g_args) = delete;

    ~GLOBALARGS(); // 析构函数声明

    void addTimeExec(std::thread::id thread_id, int64_t elapsed_time);

    void addTimeEnum(std::thread::id thread_id, int64_t elapsed_time);

    void addTimeSplit(std::thread::id thread_id, int64_t elapsed_time);

    void addCallCount(std::thread::id thread_id, int64_t new_call_count);

    void addEmbeddingCount(std::thread::id thread_id, int64_t new_embedding_count);

    void clearStatistics();

    void log4Metrics();
    void logGlobalStatistics();

    void printCandidates();

    void printEdgeMatrix();


    SPLITMODE splitMode{SPLITMODE::NOSPLIT};
    int count_unit{0};
    UnitArgs **unitArgsVec{nullptr};

    const Graph *data_graph{nullptr};
    const Graph *query_graph{nullptr};
    Edges ***edge_matrix{nullptr};
    ui **candidates{nullptr};
    ui *candidates_count{nullptr};
    TreeNode* tree{nullptr};
    ui *tree_order{nullptr};
    ui *order{nullptr};
    ui* pivots{nullptr};
    ui **bn{nullptr};
    ui *bn_count{nullptr};
    ui **weight_array{nullptr};
    std::string engine_type;

    
    int64_t output_limit_num{0};

    // statistics
    std::atomic<int64_t> embedding_count{0};
    std::atomic<ul> call_count{0};
    int64_t time_limit;
    std::atomic<bool> overtime{false};
    int total_task_num{0};

    bool needCandidates{false};
    bool needEdgeMatrix{false};

    std::vector<std::unordered_map<VertexID, std::vector<VertexID >>> TE_Candidates;
    std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> NTE_Candidates;

    std::mutex global_mutex;
    std::unordered_map<std::thread::id, int64_t> thread_times_enum;
    std::unordered_map<std::thread::id, int64_t> thread_times_exec;
    std::unordered_map<std::thread::id, int64_t> thread_times_split;
    std::unordered_map<std::thread::id, int64_t> thread_call_count;
    std::unordered_map<std::thread::id, int64_t> thread_embedding_count;
    
    int64_t load_graphs_time_in_ns{0};
    int64_t filter_vertices_time_in_ns{0};
    int64_t split_query_time_in_ns{0};
    int64_t build_table_time_in_ns{0};
    int64_t generate_query_plan_time_in_ns{0};
    int64_t enumeration_time_in_ns{0};
    int64_t preprocessing_time_in_ns{0};
    int64_t join_time_in_ns{0};
    int64_t total_time_in_ns{0};
    int64_t memory_cost_in_bytes{0};
    int64_t avg_false_positive_ratio{0};
};

struct LocalArgs {
    ui* idx{nullptr};
    ui* idx_count{nullptr};
    ui* embedding{nullptr};
    ui* idx_embedding{nullptr};
    ui* temp_buffer{nullptr};
    ui** valid_candidate_idx{nullptr};
    ui** valid_candidate{nullptr};
    bool* visited_vertices{nullptr};
    std::vector<dpiso_min_pq> vec_rank_queue;
    std::unordered_set<ui> splited_u_set;
    ui* l_order{nullptr};
    ui* u_idx_count{nullptr};
    ui* extendable{nullptr};
    ui query_vertices_num{0};
    ui max_candidates_num{0};
    ui data_vertices_num{0};
    // int max_depth{0};
    int id_unit{-1};
    int upper_depth{0};
    int cur_depth{0};

    // statistics
    int64_t l_embedding_count{0};
    int64_t l_limit_num{std::numeric_limits<int64_t>::max()};
    ul l_call_count{0};

    // 标志workload已经小于阈值了，不用再分了
    double_t upper_workload{0.0};
    bool workload_no_split_flag{false};

// #ifdef ENABLE_FROZEN_SET
    ui* temp_buffer_edges{nullptr};
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>> FUValidCandidateIdx;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui>> FUValidCandidatesCount;
// #endif

    LocalArgs() = default;
    
    // deep copy constructor
    LocalArgs(const LocalArgs& init_l_args);

    // deep copy constructor with (start_idx, end_idx, upper_depth)
    // delegate the deep copy constructor
    LocalArgs(const GLOBALARGS* g_args, const LocalArgs& parent_l_args,
            ui start_idx, ui end_idx, int upper_depth, int down_depth);

    LocalArgs(const GLOBALARGS* g_args, const LocalArgs& parent_l_args,
            int upper_depth, int down_depth, ui* calculated_valid_candidate_idx, ui idx_count_cur);
    
    ~LocalArgs(); // 析构函数声明

    void allocateBuffer(const GLOBALARGS* g_args);
};

#endif // ARGS_H