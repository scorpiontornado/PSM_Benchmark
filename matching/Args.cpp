#include "Args.h"
#include <iomanip>  // std::setw()

class TaskSlot;
// Definition of GLOBALARGS

UnitArgs::UnitArgs(InducedSubgraph* query_graph, size_t output_limit_num)
    : query_graph(query_graph), output_limit(output_limit_num) {

    vertices_count = query_graph->getVerticesCount();
    tree = new TreeNode[vertices_count];
    order = new ui[vertices_count];
// #ifdef ENABLE_FROZEN_SET
if (FROZEN_SET_FLAG) {
    u_depth = new ui[vertices_count];
}
// #endif
    tree_order = new ui[vertices_count];
    pivots = new ui[vertices_count];
    bn = new ui*[vertices_count];
    for (ui i = 0; i < vertices_count; ++i) {
        bn[i] = new ui[vertices_count];
    }
    bn_count = new ui[vertices_count];

    unit_partial_match = new PartialMatch(vertices_count, query_graph->total_vertices_count, 0);
    // LOG() << "query_graph->total_vertices_count == " << query_graph->total_vertices_count << std::endl;
    // LOG() << "vertices_count == " << vertices_count << std::endl;
    std::copy(query_graph->new2olds, query_graph->new2olds + query_graph->total_vertices_count, unit_partial_match->new2olds);
    std::copy(query_graph->old2news, query_graph->old2news + query_graph->total_vertices_count, unit_partial_match->old2news);

    /*
        对应 PSM 的 \alpha 参数
    */
    MAX_SPLIT_DEPTH = query_graph->getVerticesCount() - 3 - 1;
    // LOG the new2olds array
    // std::ostringstream oss;
    // oss << "----------printnew2olds----------" << std::endl;
    // oss << "new2olds 1: " << std::endl;
    // for (ui i = 0; i < query_graph->total_vertices_count; ++i) {
    //     oss << query_graph->new2olds[i] <<  ", ";
    // }
    // oss << std::endl;
    // oss << "new2olds 2: " << std::endl;
    // for (ui i = 0; i < query_graph->total_vertices_count; ++i) {
    //     oss << unit_partial_match->new2olds[i] <<  ", ";
    // }
    // oss << std::endl;

    // oss << "----------printold2news----------" << std::endl;
    // oss << "old2news 1: " << std::endl;
    // for (ui i = 0; i < query_graph->total_vertices_count; ++i) {
    //     oss << query_graph->old2news[i] <<  ", ";
    // }
    // oss << std::endl;
    // oss << "old2news 2: " << std::endl;
    // for (ui i = 0; i < query_graph->total_vertices_count; ++i) {
    //     oss << unit_partial_match->old2news[i] <<  ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
}

UnitArgs::~UnitArgs() {
    delete[] tree;
    delete[] order;
// #ifdef ENABLE_FROZEN_SET
if (FROZEN_SET_FLAG) {
    delete[] u_depth;
}
// #endif
    delete[] tree_order;
    delete[] pivots;
    for (ui i = 0; i < vertices_count; ++i) {
        delete[] bn[i];
    }
    delete[] bn;
    delete[] bn_count;

    // NOTE: 放到collect里面动态释放了
    // delete unit_partial_match;

    if (needCandidates){
        delete[] candidates;
        delete[] candidates_count;
    }

    if (needEdgeMatrix) {
        for (VertexID i = 0; i < vertices_count; ++i) {
            delete[] edge_matrix[i];
        }
        for (ui i = 0; i < vertices_count; ++i) {
            delete[] weight_array[i];
        }
        delete[] weight_array;
        delete[] edge_matrix;
    }
}

/*
    完成edge_matrix和candidates的映射
 */
void
UnitArgs::mapUidCandidates(GLOBALARGS* g_args) {
    needCandidates = true;
    candidates = new ui*[vertices_count];
    for (VertexID i = 0; i < vertices_count; ++i) {
        VertexID old = query_graph->new2olds[i];
        candidates[i] = g_args->candidates[old];
    }

    candidates_count = new ui[vertices_count];
    for (VertexID i = 0; i < vertices_count; ++i) {
        VertexID old = query_graph->new2olds[i];
        candidates_count[i] = g_args->candidates_count[old];
    }
}

void
UnitArgs::mapUidEdgeMatrix(GLOBALARGS* g_args) {
    weight_array = new ui*[vertices_count];
    for (ui i = 0; i < vertices_count; ++i) {
        weight_array[i] = new ui[candidates_count[i]];
        std::fill(weight_array[i], weight_array[i] + candidates_count[i], std::numeric_limits<ui>::max());
    }

    // workload_array 不需要映射，他本来就是后面才算出来的

    if (g_args->engine_type == "GQL") {
        // return;
    }

    if (g_args->engine_type == "CECI") { // copy TE_Candidates and NTE_Candidates
        TE_Candidates.resize(vertices_count);
        NTE_Candidates.resize(vertices_count);
        for (VertexID i = 0; i < vertices_count; ++i) {
            VertexID old_i = query_graph->new2olds[i];
            TE_Candidates[i] = g_args->TE_Candidates[old_i];

            NTE_Candidates[i].resize(tree[i].bn_count_);
            for (ui j = 0; j < tree[i].bn_count_; ++j) {
                VertexID newj = tree[i].bn_[j];
                VertexID old_j = query_graph->new2olds[newj];
                NTE_Candidates[i][newj] = g_args->NTE_Candidates[old_i][old_j];
            }
        }
        return;
    }

    needEdgeMatrix = true;
    edge_matrix = new Edges**[vertices_count];
    for (VertexID i = 0; i < vertices_count; ++i) {
        VertexID old_i = query_graph->new2olds[i];
        edge_matrix[i] = new Edges*[vertices_count];
        for (VertexID j = 0; j < vertices_count; ++j) {
            VertexID old_j = query_graph->new2olds[j];
            edge_matrix[i][j] = g_args->edge_matrix[old_i][old_j];
        }
    }
}

void
UnitArgs::addPartialMatch(ui* embedding) {
    std::unique_lock<std::mutex> lock(pm_mtx);
    unit_partial_match->add(embedding);
}

void
UnitArgs::generateCircinusLayer() {
    // Step 1: 计算顶点覆盖集 (vertex cover)
    // std::map<ui, VertexID> d_u;
    // for (VertexID u = 0; u < query_graph->getVerticesCount(); ++u) {
    //     d_u.emplace(query_graph->subgraph->getVertexDegree(u), u);
    // }
    for (VertexID u = 0; u < query_graph->getVerticesCount(); ++u) {
        ui nbr_cnt;
        const ui* nbrs = query_graph->subgraph->getVertexNeighbors(u, nbr_cnt);
        for (ui i = 0; i < nbr_cnt; ++i) {
            VertexID v = nbrs[i];

            // 如果这条边还没有被覆盖，选择一个端点加入顶点覆盖集
            if (edgesCovered.find({u, v}) == edgesCovered.end() && edgesCovered.find({v, u}) == edgesCovered.end()) {
                /*
                    u加入vertexCover
                    所有u相关的边加入edgesCovered
                */
                vertexCover.insert(u);
                // LOG() << "add vertex " << u << " to vertexCover" << std::endl;
                for (ui j = 0; j < nbr_cnt; ++j) {
                    VertexID tmpv = nbrs[j];
                    edgesCovered.insert({u, tmpv});
                    edgesCovered.insert({tmpv, u});
                }
                break;
            }
        }
    }    
    // std::ostringstream oss;
    // oss << "log vertexCover : " << std::endl;
    // for (auto& u : vertexCover) {
    //     oss << u << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    

    // Step 2: 根据 order 和 vertex cover，计算需要冻结的点，以及他们的有序的 fns
    /*
        需要冻结点的点需要满足以下条件：
        1. 所有bn都不在FU中
        2. 自己不在vertexCover中
     */
    for (VertexID u = 0; u < query_graph->getVerticesCount(); ++u) {
        bool isFrozen = true;
        if (vertexCover.find(u) != vertexCover.end()) {
            isFrozen = false;
            continue;
        }
        ui u_nbrs_count;
        const ui* u_nbrs = query_graph->subgraph->getVertexNeighbors(u, u_nbrs_count);
        
        for (ui i = 0; i < u_nbrs_count; ++i) {
            VertexID v = u_nbrs[i];
            if (FUSet.find(v) != FUSet.end()) {
                isFrozen = false;
                break;
            }
        }
        if (isFrozen) {
            FUSet.insert(u);
        }
    }

    // oss << "log FUSet : " << std::endl;
    // for (auto& u : FUSet) {
    //     oss << u << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // Step 3: 对于所有nfu,计算preu[nfu][bn_fus] for each bn_fu in bn_fus, for each nfu in nfus
    // 这里的构造逻辑是对fu遍历，对他们的fnu操作
    for (const VertexID& fu : FUSet) {
        TreeNode& node = tree[fu];
        std::set<VertexID> fu_fnset;
        std::vector<VertexID>& fu_fnvec = FU_fns[fu];
        for (ui i = 0; i < node.fn_count_; ++i) {
            fu_fnset.emplace(node.fn_[i]);
        }
        // 本身也加进去了
        fu_fnvec.push_back(fu);
        // 对 fu_fnset 进行排序，按照在order中的次序排序，构造出有序的 fu_fnvec
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            VertexID tmpu = order[i];
            if (fu_fnset.find(tmpu) != fu_fnset.end()) {
                fu_fnvec.push_back(tmpu);
            }
        }

        // oss << "log fu " << fu << ", fu_fnvec : " << std::endl;
        // for (auto& fnu : fu_fnvec) {
        //     oss << fnu << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");

        for (ui i = 0; i < fu_fnvec.size(); ++i) {
            VertexID fnu = fu_fnvec[i];
            if (i == 0) {
                preu[fnu][fu] = fnu;
            } else {
                preu[fnu][fu] = fu_fnvec[i-1];
            }
        }
    }

    for (const auto& [fu, fns] : FU_fns) {
        assert(fns.size() > 0);
    }

    // oss << "log preu : " << std::endl;
    // for (const auto& [u, preus] : preu) {
    //     oss << "u = " << u << ", preus = ";
    //     for (const auto& [preu, _] : preus) {
    //         oss << preu << ", ";
    //     }
    //     oss << std::endl;
    // }
    // LOG() << oss.str();
    // oss.str("");
}

void
UnitArgs::printCandidates() {
    std::ostringstream oss;
    oss << "----------printCandidates----------" << std::endl;
    for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
        oss << "u" << i << "(old = " << query_graph->new2olds[i]  << ")\'s " << candidates_count[i] << " candidates: ";
        for (ui j = 0; j < candidates_count[i]; ++j) {
            oss << candidates[i][j] << ", ";
        }
        oss << std::endl;
        // 构建完当前行的字符串后，将其一次性输出
    }
    LOG() << oss.str();
}

void
UnitArgs::printEdgeMatrix() {
    std::ostringstream oss;
    oss << "----------printEdgeMatrix----------" << std::endl;
    for (ui i = 0; i < query_graph->subgraph->getVerticesCount(); i++) {
        oss << "edge_matrix of bns of u" << i << " : " << std::endl;
        TreeNode& node = tree[i];
        for (ui nbr_idx = 0; nbr_idx < node.bn_count_; nbr_idx++) {
            VertexID j = node.bn_[nbr_idx];
            oss << "u" << query_graph->new2olds[j] << " -> u" << query_graph->new2olds[i] << ", newID : u" << j << " -> u" << i << std::endl;
            oss << "Offsets: ";
            for (ui i1 = 0; i1 < edge_matrix[j][i]->vertex_count_; i1++) {
                oss << edge_matrix[j][i]->offset_[i1] << ", ";
            }
            oss << std::endl;

            // 拼接 edge_ 中的内容
            oss << "Edges: ";
            for (ui i2 = 0; i2 < edge_matrix[j][i]->edge_count_; i2++) {
                oss << edge_matrix[j][i]->edge_[i2] << ", ";
            }
            oss << std::endl;           
        }
    }
    LOG() << oss.str();
}

void UnitArgs::printTECandidates() {
    std::ostringstream oss;
    oss << "========== TE_Candidates PRINT ==========" << std::endl;

    for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
        oss << "u" << i << ":\n";

        if (TE_Candidates[i].empty()) {
            oss << "  (empty)\n";
            continue;
        }

        for (const auto& [v_p, vec] : TE_Candidates[i]) {
            oss << "  v_p = " << std::setw(4) << v_p << " -> { ";

            // 格式化邻居列表，去除最后的逗号
            for (size_t j = 0; j < vec.size(); j++) {
                oss << vec[j];
                if (j < vec.size() - 1) {
                    oss << ", ";
                }
            }
            oss << " }" << std::endl;
        }
    }

    oss << "=========================================" << std::endl;
    LOG() << oss.str();
}
GLOBALARGS::GLOBALARGS(const Graph *data_graph, const Graph *query_graph,
    Edges ***edge_matrix, ui **candidates, ui *candidates_count,
    size_t output_limit_num, int64_t time_limit) :
    data_graph(data_graph), query_graph(query_graph), edge_matrix(edge_matrix),
    candidates(candidates), candidates_count(candidates_count),
    output_limit_num(output_limit_num), time_limit(time_limit) { }

// 析构函数实现
GLOBALARGS::~GLOBALARGS() {
    // LOG() << "call GLOBALARGS's destructor" << std::endl;
    if (engine_type != "CECI") {
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            ui u_nbrs_count;
            const VertexID* u_nbrs = query_graph->getVertexNeighbors(i, u_nbrs_count);
            for (ui j = 0; j < u_nbrs_count; ++j) {
                if (edge_matrix[i][u_nbrs[j]] != nullptr) {
                    delete edge_matrix[i][u_nbrs[j]];
                }
            }
            delete[] edge_matrix[i];
        }
    }
    delete[] edge_matrix;

    if (needCandidates) {
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            delete[] candidates[i];
        }
    }
    delete[] candidates;
    delete[] candidates_count;
    // delete data_graph;
    // delete query_graph;
}

void
GLOBALARGS::addTimeEnum(std::thread::id thread_id, int64_t elapsed_time) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (thread_times_enum.find(thread_id) == thread_times_enum.end()) {
        thread_times_enum[thread_id] = 0;
    }
    thread_times_enum[thread_id] += elapsed_time;
}

void
GLOBALARGS::addTimeExec(std::thread::id thread_id, int64_t elapsed_time) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (thread_times_exec.find(thread_id) == thread_times_exec.end()) {
        thread_times_exec[thread_id] = 0;
    }
    thread_times_exec[thread_id] += elapsed_time;
}

void
GLOBALARGS::addTimeSplit(std::thread::id thread_id, int64_t elapsed_time) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (thread_times_split.find(thread_id) == thread_times_split.end()) {
        thread_times_split[thread_id] = 0;
    }
    thread_times_split[thread_id] += elapsed_time;
}

void
GLOBALARGS::addCallCount(std::thread::id thread_id, int64_t new_call_count) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (thread_call_count.find(thread_id) == thread_call_count.end()) {
        thread_call_count[thread_id] = 0;
    }
    thread_call_count[thread_id] += new_call_count;
}

void
GLOBALARGS::addEmbeddingCount(std::thread::id thread_id, int64_t new_embedding_count) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (thread_embedding_count.find(thread_id) == thread_embedding_count.end()) {
        thread_embedding_count[thread_id] = 0;
    }
    thread_embedding_count[thread_id] += new_embedding_count;
}

void
GLOBALARGS::clearStatistics() {
    thread_times_exec.clear();
    thread_times_split.clear();
    thread_call_count.clear();
    thread_embedding_count.clear();
}

void
GLOBALARGS::log4Metrics() {
    // 根据 thread_times_enum 建立 thread::id 映射编号
    std::unordered_map<std::thread::id, int> thread_id_to_index;
    int index = 0;
    for (const auto& [tid, _] : thread_times_enum) {
        thread_id_to_index[tid] = index++;
    }

    // 自动补充缺失的key为0
    auto ensure_keys_exist = [&](std::unordered_map<std::thread::id, int64_t>& map) {
        for (const auto& [tid, _] : thread_id_to_index) {
            map.try_emplace(tid, 0);
        }
    };

    ensure_keys_exist(thread_times_exec);
    ensure_keys_exist(thread_times_split);
    ensure_keys_exist(thread_call_count);
    ensure_keys_exist(thread_embedding_count);

    // 计算idle时间
    std::unordered_map<std::thread::id, int64_t> thread_times_idle;
    for (const auto& [tid, enum_time] : thread_times_enum) {
        // 假设 enumeration_time_in_ns 是单线程总执行时间
        // 则该线程 idle = enumeration_time_in_ns - thread_times_exec[tid]
        thread_times_idle[tid] = enum_time - thread_times_exec[tid];
    }

    // 打印函数
    auto print_mapped_data = [&](const std::string& title, const std::unordered_map<std::thread::id, int64_t>& data_map, bool convert_to_sec = true) {
        std::ostringstream oss;
        oss << title;
        bool first = true;
        for (const auto& [tid, value] : data_map) {
            if (first) {
                // 第一次打印，前面不加逗号
                first = false;
                if (convert_to_sec)
                    oss << " " << NANOSECTOSEC(value);
                else
                    oss << " " << value;
            } else {
                // 后续打印需要加逗号
                if (convert_to_sec)
                    oss << ", " << NANOSECTOSEC(value);
                else
                    oss << ", " << value;
            }
        }
        LOG() << oss.str() << std::endl;
    };

    print_mapped_data("Thread Enum Time:", thread_times_enum);
    print_mapped_data("Thread Execution Time:", thread_times_exec);
    print_mapped_data("Thread Split Time:", thread_times_split);
    print_mapped_data("Thread Idle Time:", thread_times_idle);
    print_mapped_data("Thread Call Count:", thread_call_count, false);
    print_mapped_data("Thread Embedding Count:", thread_embedding_count, false);
    LOG() << "TaskNum: " << total_task_num << std::endl;
}

void
GLOBALARGS::logGlobalStatistics() {
    LOG() << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
    LOG() << "Build table time (seconds): " << NANOSECTOSEC(build_table_time_in_ns) << std::endl;
    LOG() << "Filter vertices time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
    LOG() << "Split Qeury Pattern Time: " << NANOSECTOSEC(split_query_time_in_ns) << std::endl;
    LOG() << "Generate query plan time (seconds): " << NANOSECTOSEC(generate_query_plan_time_in_ns) << std::endl;
    LOG() << "Enumerate time (seconds): " << NANOSECTOSEC(enumeration_time_in_ns) << std::endl;
    LOG() << "Preprocessing time (seconds): " << NANOSECTOSEC(preprocessing_time_in_ns) << std::endl;
    LOG() << "Join time (seconds): " << NANOSECTOSEC(join_time_in_ns) << std::endl;
    LOG() << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
    LOG() << "Memory cost (MB): " << BYTESTOMB(memory_cost_in_bytes) << std::endl;
    LOG() << "#Embeddings: " << embedding_count << std::endl;
    LOG() << "Call Count: " << call_count.load() << std::endl;
    log4Metrics();
    LOG() << "Per Call Time (nanoseconds): " << enumeration_time_in_ns / (call_count.load() == 0 ? 1 : call_count.load()) << std::endl;
    LOG() << "Overtime: " << overtime.load() << std::endl; // overtime = 1 means timeout
    LOG() << "End the query" << std::endl;
}

void
GLOBALARGS::printCandidates() {
    std::ostringstream oss;
    oss << "----------printCandidates----------" << std::endl;
    for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
        oss << "u" << i << "\'s " << candidates_count[i] << " candidates: ";
        for (ui j = 0; j < candidates_count[i]; ++j) {
            oss << candidates[i][j] << ", ";
        }
        oss << std::endl;
        // 构建完当前行的字符串后，将其一次性输出
    }
    LOG() << oss.str();
}

void
GLOBALARGS::printEdgeMatrix() {
    std::ostringstream oss;
    oss << "----------printEdgeMatrix----------" << std::endl;
    for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
        oss << "edge_matrix of bns of u" << i << " : " << std::endl;
        TreeNode& node = tree[i];
        for (ui nbr_idx = 0; nbr_idx < node.bn_count_; nbr_idx++) {
            VertexID j = node.bn_[nbr_idx];
            oss << "u" << j << " -> u" << i << std::endl;
            oss << "Offsets: ";
            for (ui i1 = 0; i1 < edge_matrix[j][i]->vertex_count_; i1++) {
                oss << edge_matrix[j][i]->offset_[i1] << ", ";
            }
            oss << std::endl;

            // 拼接 edge_ 中的内容
            oss << "Edges: ";
            for (ui i2 = 0; i2 < edge_matrix[j][i]->edge_count_; i2++) {
                oss << edge_matrix[j][i]->edge_[i2] << ", ";
            }
            oss << std::endl;



            // 打印反向的
            oss << "u" << i << " -> u" << j << std::endl;
            oss << "Offsets: ";
            for (ui i1 = 0; i1 < edge_matrix[i][j]->vertex_count_; i1++) {
                oss << edge_matrix[i][j]->offset_[i1] << ", ";
            }
            oss << std::endl;

            // 拼接 edge_ 中的内容
            oss << "Edges: ";
            for (ui i2 = 0; i2 < edge_matrix[i][j]->edge_count_; i2++) {
                oss << edge_matrix[i][j]->edge_[i2] << ", ";
            }
            oss << std::endl;           
        }
    }
    LOG() << oss.str();   
}



// Definition of LocalArgs
// NOTE: 在localArgs的allocateBuffer之后，需要对extendable用unitArgs的tree来赋
void
LocalArgs::allocateBuffer(const GLOBALARGS* g_args) {
    // LOG() << "call LocalArgs's allocateBuffer" << std::endl;
    // ui unit_query_vertices_num = g_args->unitArgsVec[id_unit]->query_graph->getVerticesCount();
    ui query_vertices_num = g_args->query_graph->getVerticesCount();
    ui data_vertices_num = g_args->data_graph->getVerticesCount();
    ui max_candidates_num = g_args->candidates_count[0];

    // NOTE: 这里的query_vertices_num要用原本的graph，因为max_candidates_num是要在全局来看的，candidates也是全局来看的
    for (ui i = 1; i < query_vertices_num; ++i) {
        VertexID cur_vertex = i;
        max_candidates_num = std::max(max_candidates_num, g_args->candidates_count[cur_vertex]);
    }
    idx = new ui[query_vertices_num];
    idx_count = new ui[query_vertices_num];
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    // LOG() << "query_vertices_num == " << query_vertices_num << std::endl;
    u_idx_count = new ui[query_vertices_num]();
    l_order = new ui[query_vertices_num];
    extendable = new ui[query_vertices_num];
}
// #endif
    embedding = new ui[query_vertices_num];
    idx_embedding = new ui[query_vertices_num];
    visited_vertices = new bool[data_vertices_num];
    temp_buffer = new ui[max_candidates_num];
// #ifdef ENABLE_FROZEN_SET
if (FROZEN_SET_FLAG) {
    temp_buffer_edges = new ui[max_candidates_num];
}
// #endif
    valid_candidate_idx = new ui*[query_vertices_num];
    valid_candidate = new ui*[query_vertices_num];
    // LOG() << "query_vertices_num == " << query_vertices_num << std::endl;
    // LOG() << "max_candidates_num == " << max_candidates_num << std::endl;
    for (ui i = 0; i < query_vertices_num; ++i) {
        valid_candidate_idx[i] = new ui[max_candidates_num];
    }

    for (ui i = 0; i < query_vertices_num; ++i) {
        valid_candidate[i] = new ui[max_candidates_num];
    }

    std::fill(visited_vertices, visited_vertices + data_vertices_num, false);
    this->query_vertices_num = query_vertices_num;
    this->max_candidates_num = max_candidates_num;
    this->data_vertices_num = data_vertices_num;

    // this->max_depth = g_args->query_graph->getVerticesCount();
}

// 深拷贝构造函数实现
LocalArgs::LocalArgs(const LocalArgs& init_l_args) {
    // LOG() << "call LocalArgs's deep copy constructor" << std::endl;
    query_vertices_num = init_l_args.query_vertices_num;
    // LOG() << "copy constructor's query_vertices_num == " << query_vertices_num << std::endl;
    max_candidates_num = init_l_args.max_candidates_num;
    data_vertices_num = init_l_args.data_vertices_num;
    id_unit = init_l_args.id_unit;
    
    idx = new ui[query_vertices_num];
    idx_count = new ui[query_vertices_num];
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    u_idx_count = new ui[query_vertices_num]();
    // LOG() << "u_idx_count == " << u_idx_count << std::endl;
    l_order = new ui[query_vertices_num];
    extendable = new ui[query_vertices_num];
}
// #endif
    embedding = new ui[query_vertices_num];
    idx_embedding = new ui[query_vertices_num];
    visited_vertices = new bool[data_vertices_num];
    temp_buffer = new ui[max_candidates_num];
// #ifdef ENABLE_FROZEN_SET
if (FROZEN_SET_FLAG) {
    temp_buffer_edges = new ui[max_candidates_num];
}
// #endif
    valid_candidate_idx = new ui *[query_vertices_num];
    valid_candidate = new ui *[query_vertices_num];
    
    // TODO: 0~upper_depth-1的部分是不需要赋值的
    // 这里的i实际上对应的depth而不是uid
    // LOG() << "query_vertices_num == " <<query_vertices_num << std::endl; 
    // LOG() << "max_candidates_num == " << max_candidates_num << std::endl;
    for (ui i = 0; i < query_vertices_num; ++i) {
        valid_candidate_idx[i] = new ui[max_candidates_num];
        // LOG() << "create valid_candidate_idx[" << i << "] == " << valid_candidate_idx[i] << std::endl;

    }
    for (ui i = 0; i < query_vertices_num; ++i) {
        valid_candidate[i] = new ui[max_candidates_num];
        // LOG() << "create valid_candidate[" << i << "] == " << valid_candidate[i] << std::endl;
    }
    std::memcpy(idx, init_l_args.idx, query_vertices_num * sizeof(ui));
    std::memcpy(idx_count, init_l_args.idx_count, query_vertices_num * sizeof(ui));
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    // 这里复制u_idx_count是有必要的，有很多的extendable==0但是不是cur_u的点，是需要复制的
    std::memcpy(u_idx_count, init_l_args.u_idx_count, query_vertices_num * sizeof(ui));
    std::memcpy(l_order, init_l_args.l_order, query_vertices_num * sizeof(ui));
}
// #endif
    std::memcpy(embedding, init_l_args.embedding, query_vertices_num * sizeof(ui));
    std::memcpy(idx_embedding, init_l_args.idx_embedding, query_vertices_num * sizeof(ui));
    std::memcpy(visited_vertices, init_l_args.visited_vertices, data_vertices_num * sizeof(bool));
    
    // NOTE: 这一部分的复制是没有必要的
    // for (ui i = 0; i < query_vertices_num; ++i) {
    //     std::memcpy(valid_candidate_idx[i], init_l_args.valid_candidate_idx[i], max_candidates_num * sizeof(ui));
    // }
}

// end_idx should be the idx 'after' the last element
LocalArgs::LocalArgs(const GLOBALARGS* g_args, const LocalArgs& parent_l_args,
                    ui start_idx, ui end_idx, int upper_depth, int down_depth)
    : LocalArgs(parent_l_args) {  // 委托调用已有的深拷贝构造函数
    assert(upper_depth <= down_depth);
    // LOG() << "call LocalArgs's copy constructor" << std::endl;
    this->upper_depth = upper_depth;
    // LOG() << "end_idx = " << end_idx << ", start_idx = " << start_idx << std::endl;

if (FAILING_SET_FLAG) {
// #ifdef ENABLE_FAILING_SET
    VertexID cur_u = parent_l_args.l_order[upper_depth];
    u_idx_count[cur_u] = end_idx - start_idx;
    std::memset(idx, 0, query_vertices_num * sizeof(ui));
    // LOG() << "cur_u == " << cur_u << std::endl;
    // LOG() << "ptr of u_idx_count == " << u_idx_count << std::endl;
    // LOG() << "copy u_idx_count[" << cur_u << "] = " << u_idx_count[cur_u] << std::endl;
    
    std::memcpy(
        valid_candidate_idx[cur_u],
        parent_l_args.valid_candidate_idx[cur_u] + start_idx,
        u_idx_count[cur_u] * sizeof(ui)
    );

    // std::ostringstream oss;
    // oss << "copy valid_candidate_idx : " << std::endl;
    // for (ui i = 0; i < u_idx_count[cur_u]; ++i) {
    //     oss << valid_candidate_idx[cur_u][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // 这里要按照l_order来恢复visited
    // bool flag = true;
    for (int depth = upper_depth; depth < down_depth; ++depth) {
        VertexID u = l_order[depth];
        if (!visited_vertices[embedding[u]]) {
            LOG() << "Error: u == " << u << ", embedding[u] == " << embedding[u] << std::endl;
            // flag = false;
            abort();
        }
        visited_vertices[embedding[u]] = false;
    }
} else {
// #else
    // LOG() << "idx_count[" << upper_depth << "] = " << end_idx - start_idx << std::endl;
    idx_count[upper_depth] = end_idx - start_idx;
    std::memset(idx, 0, query_vertices_num * sizeof(ui));
    std::memcpy(
        valid_candidate_idx[upper_depth],
        parent_l_args.valid_candidate_idx[upper_depth] + start_idx,
        idx_count[upper_depth] * sizeof(ui)
    );
    std::memcpy(
        valid_candidate[upper_depth],
        parent_l_args.valid_candidate[upper_depth] + start_idx,
        idx_count[upper_depth] * sizeof(ui)
    );

    id_unit = parent_l_args.id_unit;
    UnitArgs* u_args = g_args->unitArgsVec[id_unit];
    ui* order = u_args->order;
    for (int depth = upper_depth; depth <= down_depth; depth++) {
        ui u = order[depth];
        visited_vertices[embedding[u]] = false;
    }

}
// #endif
}

/*
    这是一个新的LocalArgs构造函数，用 calculated_valid_candidate_idx 和 idx_count_cur 来赋值
    而不是之前的在原有的 valid_candidate_idx 基础上用 start_idx 和 end_idx
*/
LocalArgs::LocalArgs(const GLOBALARGS* g_args, const LocalArgs& parent_l_args,
                     int upper_depth, int down_depth, ui* calculated_valid_candidate_idx, ui idx_count_cur)
    : LocalArgs(parent_l_args) {  // 委托调用已有的深拷贝构造函数
    assert(upper_depth <= down_depth);
    // LOG() << "call LocalArgs's copy constructor" << std::endl;
    this->upper_depth = upper_depth;
    // LOG() << "end_idx = " << end_idx << ", start_idx = " << start_idx << std::endl;

if (FAILING_SET_FLAG) {
// #ifdef ENABLE_FAILING_SET
    VertexID cur_u = parent_l_args.l_order[upper_depth];
    u_idx_count[cur_u] = idx_count_cur;
    std::memset(idx, 0, query_vertices_num * sizeof(ui));
    // LOG() << "cur_u == " << cur_u << std::endl;
    // LOG() << "ptr of u_idx_count == " << u_idx_count << std::endl;
    // LOG() << "copy u_idx_count[" << cur_u << "] = " << u_idx_count[cur_u] << std::endl;

    // NOTE: 改造为拷贝传入进来的就可以
    std::memcpy(
        valid_candidate_idx[cur_u],
        calculated_valid_candidate_idx,
        u_idx_count[cur_u] * sizeof(ui)
    );

    // std::ostringstream oss;
    // oss << "copy valid_candidate_idx : " << std::endl;
    // for (ui i = 0; i < u_idx_count[cur_u]; ++i) {
    //     oss << valid_candidate_idx[cur_u][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // 这里要按照l_order来恢复visited
    // bool flag = true;
    for (int depth = upper_depth; depth < down_depth; ++depth) {
        VertexID u = l_order[depth];
        if (!visited_vertices[embedding[u]]) {
            LOG() << "Error: u == " << u << ", embedding[u] == " << embedding[u] << std::endl;
            // flag = false;
            abort();
        }
        visited_vertices[embedding[u]] = false;
    }
} else {
// #else
    // LOG() << "idx_count[" << upper_depth << "] = " << end_idx - start_idx << std::endl;
    idx_count[upper_depth] = idx_count_cur;
    std::memset(idx, 0, query_vertices_num * sizeof(ui));

    // TODO: 这两个部分都不用，直接拷贝传入进来的就可以
    std::memcpy(
        valid_candidate_idx[upper_depth],
        calculated_valid_candidate_idx,
        idx_count[upper_depth] * sizeof(ui)
    );

    std::memcpy(
        valid_candidate[upper_depth],
        calculated_valid_candidate_idx,
        idx_count[upper_depth] * sizeof(ui)
    );

    // 恢复visited_vertices
    id_unit = parent_l_args.id_unit;
    UnitArgs* u_args = g_args->unitArgsVec[id_unit];
    ui* order = u_args->order;
    for (int depth = upper_depth; depth <= down_depth; depth++) {
        ui u = order[depth];
if (FROZEN_SET_FLAG) {
        if (parent_l_args.FUValidCandidateIdx.find(u) == parent_l_args.FUValidCandidateIdx.end()) {
            visited_vertices[embedding[u]] = false;    
        }
} else {
        visited_vertices[embedding[u]] = false;
}
    }

}
// #endif
}


// 析构函数实现
LocalArgs::~LocalArgs() {
    // LOG() << "call destructor of LocalArgs" << std::endl;
    // LOG() << "temp_buffer == " << temp_buffer << std::endl;
    // LOG() << "temp_buffer_edge  == " << temp_buffer_edges << std::endl;
    // std::cout << "delete idx == " << idx << std::endl;
    delete[] idx;
    // std::cout << "delete idx_count == " << idx_count << std::endl;
    delete[] idx_count;
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG) {
    // std::cout << "delete u_idx_count == " << u_idx_count << std::endl;
    delete[] u_idx_count;
    // std::cout << "delete l_order == " << l_order << std::endl;
    delete[] l_order;
    // std::cout << "delete extendable == " << extendable << std::endl;
    delete[] extendable;
}
// #endif
    // std::cout << "delete embedding == " << embedding << std::endl;
    delete[] embedding;
    // std::cout << "delete idx_embedding == " << idx_embedding << std::endl;
    delete[] idx_embedding;
    // std::cout << "delete visited_vertices == " << visited_vertices << std::endl;
    delete[] visited_vertices;
    // std::cout << "delete temp_buffer == " << temp_buffer << std::endl;
    delete[] temp_buffer;

// #ifdef ENABLE_FROZEN_SET
if (FROZEN_SET_FLAG) {
    delete[] temp_buffer_edges;
    for (auto& [fu, valid_candidate_idx_fu] : FUValidCandidateIdx) {
        for (auto& [fnu, valid_candidate_idx_fnu] : valid_candidate_idx_fu) {
            // std::cout << "delete valid_candidate_idx_fnu == " << valid_candidate_idx_fnu << std::endl;
            delete[] valid_candidate_idx_fnu;
        }
    }
}
// #endif

    for (size_t i = 0; i < query_vertices_num; i++) {
        // LOG() << "delete i " << i << " of " << query_vertices_num << std::endl;
        // std::cout << "delete valid_candidate_idx[" << i << "] == " << valid_candidate_idx[i] << std::endl;
        delete[] valid_candidate_idx[i];
        
        // LOG() << "delete ok." << std::endl;
    }

    for (size_t i = 0; i < query_vertices_num; i++) {
        // // std::cout << "delete valid_candidate[" << i << "] == " << valid_candidate[i] << std::endl;
        delete[] valid_candidate[i];
    }

    delete[] valid_candidate_idx;
    delete[] valid_candidate;
}