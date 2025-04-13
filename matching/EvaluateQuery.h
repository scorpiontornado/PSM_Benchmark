#ifndef SUBGRAPHMATCHING_EVALUATEQUERY_H
#define SUBGRAPHMATCHING_EVALUATEQUERY_H

#include "graph/graph.h"
#include "utility/QFliter.h"
#include "TaskPool.h"
#include <vector>
#include <queue>
#include <bitset>

// // Min priority queue.
// static const auto extendable_vertex_compare = [](std::pair<std::pair<VertexID, ui>, ui> l, std::pair<std::pair<VertexID, ui>, ui> r) {
//     if (l.first.second == 1 && r.first.second != 1) {
//         return true;
//     }
//     else if (l.first.second != 1 && r.first.second == 1) {
//         return false;
//     }
//     else
//     {
//         return l.second > r.second;
//     }
// };

// typedef std::priority_queue<std::pair<std::pair<VertexID, ui>, ui>, std::vector<std::pair<std::pair<VertexID, ui>, ui>>,
//         decltype(extendable_vertex_compare)> dpiso_min_pq;

class EvaluateQuery {
public:

    static size_t ParallelExecute(TaskPool* taskPool, SplitGraph* splitGraph, UnitArgs** unitArgsVec);
    static void SingleExecute(TaskPool* taskPool);
    static void handleSingleExecute(std::chrono::steady_clock::time_point enum_start);
    static void handleTaskResp(int64_t l_embedding_count, ul l_call_count, std::chrono::steady_clock::time_point& time);
    static void resetExeTime(std::chrono::steady_clock::time_point& time);
    static void LFTJ(TaskPool* taskPool, TaskSlot& task, int task_id);
    static void exploreCircinusStyle(TaskPool* taskPool, TaskSlot& task, int task_id);
    static void exploreGraphQLStyle(TaskPool* taskPool, TaskSlot& task, int task_id);
    static void exploreGraph(TaskPool* taskPool, TaskSlot& task, int task_id);
    static void exploreDPisoStyle(TaskPool* taskPool, TaskSlot& task, int task_id);
    static void exploreCECIStyle(TaskPool* taskPool, TaskSlot& task, int task_id);

#if ENABLE_QFLITER == 1
    static BSRGraph*** qfliter_bsr_graph_;
    static int* temp_bsr_base1_;
    static int* temp_bsr_state1_;
    static int* temp_bsr_base2_;
    static int* temp_bsr_state2_;
#endif

#ifdef SPECTRUM
    static bool exit_;
#endif

#ifdef DISTRIBUTION
    static size_t* distribution_count_;
#endif
    static void generateBN(const Graph *query_graph, ui *order, ui *pivot, ui** bn, ui* bn_count);
    static void generateBN(const Graph *query_graph, ui *order, ui** bn, ui* bn_count);
    static void generateBN(UnitArgs* u_args);
    static void generateBNWithPivot(UnitArgs* u_args);

private:
    static void allocateBuffer(const GLOBALARGS* g_args, LocalArgs* l_args);
    static void releaseBuffer(ui query_vertices_num, ui *idx, ui *idx_count, ui *embedding, ui *idx_embedding,
                                  ui *temp_buffer, ui **valid_candidate_idx, bool *visited_vertices, ui **bn, ui *bn_count);

    static void generateValidCandidateIndex(const Graph *data_graph, ui depth, ui *embedding, ui *idx_embedding,
                                            ui *idx_count, ui **valid_candidate_index, Edges ***edge_matrix,
                                            bool *visited_vertices, ui **bn, ui *bn_cnt, ui *order, ui *pivot,
                                            ui **candidates);
    static void  generateValidCandidateIndexWithPivot(const Graph* data_graph, ui depth, TaskSlot &task);

    static void generateValidCandidateIndex(ui depth, ui *idx_embedding, ui *u_idx_count, ui **valid_candidate_index,
                                                Edges ***edge_matrix, ui **bn, ui *bn_cnt, ui *order, ui *&temp_buffer);
    static void generateValidCandidateIndex(ui depth, TaskSlot &task);
    static void refineFUVCIS(ui depth, TaskSlot &task, bool& emptyFlag);
    static void generateValidCandidateIndexUseUnion(ui depth, TaskSlot &task);

    static void updateFrozenCandidateIndex(ui depth, TaskSlot &task, ui* u_depth,
        std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>> FUValidCandidateIdx,
        std::unordered_map<VertexID, std::unordered_map<VertexID, ui>> FUValidCandidatesCount);

    static void generateValidCandidates(const Graph* data_graph, ui depth, TaskSlot& task);

    static void generateValidCandidates(ui depth, TaskSlot& task);

    static void generateValidCandidates(const Graph *query_graph, const Graph *data_graph, ui depth, ui *embedding,
                                            ui *idx_count, ui **valid_candidate, bool *visited_vertices, ui **bn, ui *bn_cnt,
                                            ui *order, ui *pivot);
    static void generateValidCandidates(ui depth, ui *embedding, ui *idx_count, ui **valid_candidates, ui *order,
                                            ui *&temp_buffer, TreeNode *tree,
                                            std::vector<std::unordered_map<VertexID, std::vector<VertexID>>> &TE_Candidates,
                                            std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> &NTE_Candidates);

    static void updateExtendableVertex(VertexID mapped_vertex, TaskSlot& task, UnitArgs* u_args);
    // static void updateExtendableVertex(ui *idx_embedding, ui *idx_count, ui **valid_candidate_index,
    //                                       Edges ***edge_matrix, ui *&temp_buffer, ui **weight_array,
    //                                       TreeNode *tree, VertexID mapped_vertex, ui *extendable,
    //                                       std::vector<dpiso_min_pq> &vec_rank_queue, const Graph *query_graph);

    static void restoreExtendableVertex(TreeNode* tree, VertexID unmapped_vertex, ui *extendable);
    static void generateValidCandidateIndex(VertexID vertex, ui *idx_embedding, ui *idx_count, ui *&valid_candidate_index,
                                            Edges ***edge_matrix, ui *bn, ui bn_cnt, ui *&temp_buffer);

    static void computeAncestor(const Graph *query_graph, TreeNode *tree, VertexID *order,
                                std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors);

    static void computeAncestor(const Graph *query_graph, ui** bn, ui* bn_cnt, VertexID *order,
                                std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors);

    static void computeAncestor(const Graph *query_graph, VertexID *order, std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors);

    static ui bsxSepDiff(std::vector<VertexID> &v_cans, const ui *indep_con_cnt, int forward_idx, int backward_idx);
    static void bsxEnumerate4Parts(ui **&sep_flags, const VertexID* nodes, ui nodes_num,
                                   std::vector<std::vector<VertexID>>& cans, bool *&visited_v,
                                   uint64_t &cur_cnt, ui* idx, ui* cnt, ui* un_con_cnt, uint64_t* embedding_level);
    static void bsxGenResult(TaskSlot& task, ui *indep_con_cnt, ui** sep_flag, ui* idx, ui* cnt, ui* un_con_cnt, uint64_t* embedding_level);
    static void productAll(TaskSlot& task, std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>>& FUValidCandidateIdx,
            std::unordered_map<VertexID, std::unordered_map<VertexID, ui>>& FUValidCandidatesCount, ui* frozen_depthes, ui frozen_count, ui* fidx,
            const std::chrono::_V2::steady_clock::time_point start_time_exec, TaskPool* taskPool);

    static std::bitset<MAXIMUM_QUERY_GRAPH_SIZE> exploreDPisoBacktrack(ui max_depth, ui depth, VertexID mapped_vertex, TreeNode *tree, ui *idx_embedding,
                                                     ui *embedding, std::unordered_map<VertexID, VertexID> &reverse_embedding,
                                                     bool *visited_vertices, ui *idx_count, ui **valid_candidate_index,
                                                     Edges ***edge_matrix,
                                                     std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors,
                                                     dpiso_min_pq rank_queue, ui **weight_array, ui *&temp_buffer, ui *extendable,
                                                     ui **candidates, size_t &embedding_count, size_t &call_count,
                                                     const Graph *query_graph);

public:
    static std::function<void(TaskSlot&, int)> enum_method;
};


#endif //SUBGRAPHMATCHING_EVALUATEQUERY_H
