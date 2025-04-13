#ifndef SUBGRAPHMATCHING_GENERATEQUERYPLAN_H
#define SUBGRAPHMATCHING_GENERATEQUERYPLAN_H

#include "graph/graph.h"
#include <vector>
#include "Args.h"
class GenerateQueryPlan {
public:

    static void computeWeightArray(ui query_vertices_num, const ui* tree_order, TreeNode* tree,
                            Edges*** edge_matrix, const ui* candidates_count, ui** weight_array);

    static void computeTotalWorkload(UnitArgs* u_args);

    static void initWorkload(ui query_vertices_num, const ui* tree_order, const ui* candidates_count, ui** candidates,
                    std::vector<std::unordered_map<VertexID, int64_t>>& workload_array);

    static void computeWorkloadArray(UnitArgs *u_args, ui query_vertices_num, const ui* tree_order, TreeNode* tree,
        Edges*** edge_matrix, const ui* candidates_count, ui** candidates,
        std::vector<std::unordered_map<VertexID, int64_t>>& workload_array);

    static void computeWorkloadArray(UnitArgs *u_args, ui query_vertices_num, const ui* tree_order, TreeNode* tree,
        std::vector<std::unordered_map<VertexID, std::vector<VertexID >>>& TE_Candidates, const ui* candidates_count, ui** candidates,
        std::vector<std::unordered_map<VertexID, int64_t>>& workload_array);

    static void computeWorkloadArray(UnitArgs *u_args, ui query_vertices_num, const ui* tree_order, TreeNode* tree,
        const Graph* data_graph, const ui* candidates_count, ui** candidates,
        std::vector<std::unordered_map<VertexID, int64_t>>& workload_array);

    static void printWorkloadArray(Graph* q_graph, UnitArgs* u_args);

    static void generateGQLQueryPlan(const Graph *data_graph, const Graph *query_graph, UnitArgs *u_args);
    static void generateQSIQueryPlan(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix,
                                         ui *&order, ui *&pivot);

    static void generateRIQueryPlan(const Graph *data_graph, const Graph* query_graph, ui *&order, ui *&pivot);

    static void generateVF2PPQueryPlan(const Graph* data_graph, const Graph *query_graph, ui *&order, ui *&pivot);

    static void generateOrderSpectrum(const Graph* query_graph, std::vector<std::vector<ui>>& spectrum, ui num_spectrum_limit);

    static void
    generateTSOQueryPlan(const Graph *query_graph, Edges ***edge_matrix, ui *&order, ui *&pivot,
                             TreeNode *tree, ui *dfs_order);

    static void
    generateCFLQueryPlan(const Graph *data_graph, const Graph *query_graph, Edges ***edge_matrix,
                             ui *&order, ui *&pivot, TreeNode *tree, ui *bfs_order, ui *candidates_count);
    static void generateCFLQueryPlan(const Graph *query_graph, UnitArgs* u_args);

    static void
    generatePivot(UnitArgs* u_args);

    static void
    generateDPisoQueryPlan(const Graph *query_graph, UnitArgs* unitArgs);

    static void generateCECIQueryPlan(const Graph* query_graph, TreeNode *tree, ui *bfs_order, ui *&order, ui *&pivot);
    static void generateCECIQueryPlan(const Graph *query_graph, UnitArgs* u_args);
    static void checkQueryPlanCorrectness(const Graph* query_graph, ui* order, ui* pivot);

    static void checkQueryPlanCorrectness(const Graph* query_graph, ui* order);

    static void printQueryPlan(const Graph* query_graph, ui* order);

    static void printSimplifiedQueryPlan(const Graph* query_graph, ui* order);
private:
    static VertexID selectGQLStartVertex(const Graph *query_graph, ui *candidates_count);
    static std::pair<VertexID, VertexID> selectQSIStartEdge(const Graph *query_graph, Edges ***edge_matrix);

    static void generateRootToLeafPaths(TreeNode *tree_node, VertexID cur_vertex, std::vector<ui> &cur_path,
                                        std::vector<std::vector<ui>> &paths);
    static void estimatePathEmbeddsingsNum(std::vector<ui> &path, Edges ***edge_matrix,
                                           std::vector<size_t> &estimated_embeddings_num);

    static void generateCorePaths(const Graph* query_graph, TreeNode* tree_node, VertexID cur_vertex, std::vector<ui> &cur_core_path,
                                  std::vector<std::vector<ui>> &core_paths);

    static void generateTreePaths(const Graph* query_graph, TreeNode* tree_node, VertexID cur_vertex,
                                  std::vector<ui> &cur_tree_path, std::vector<std::vector<ui>> &tree_paths);

    static void generateLeaves(const Graph* query_graph, std::vector<ui>& leaves);

    static ui generateNoneTreeEdgesCount(const Graph *query_graph, TreeNode *tree_node, std::vector<ui> &path);
    static void updateValidVertices(const Graph* query_graph, VertexID query_vertex, std::vector<bool>& visited, std::vector<bool>& adjacent);
};


#endif //SUBGRAPHMATCHING_GENERATEQUERYPLAN_H
