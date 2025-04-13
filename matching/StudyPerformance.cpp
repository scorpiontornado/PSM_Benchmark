#include <chrono>
#include <future>
#include <thread>
#include <fstream>
#include <sstream>

#include "matchingcommand.h"
#include "graph/graph.h"
#include "utility/splitGraph/SplitGraph.h"
#include "utility/relations/partialMatch.h"
#include "JoinCollect.h"
#include "GenerateFilteringPlan.h"
#include "FilterVertices.h"
#include "BuildTable.h"
#include "GenerateQueryPlan.h"
#include "EvaluateQuery.h"
#include "TaskPool.h"

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// std::string extract_graph_file(std::string input_file, int num) {
//     // 从右边开始查找第五个斜杠的位置
//     size_t pos = input_file.size();
//     int slash_count = 0;
    
//     while (slash_count < num) {
//         pos = input_file.rfind('/', pos - 1); // 向左查找下一个斜杠
//         if (pos == std::string::npos) {
//             LOG() << "Error: 路径中斜杠数量不足, 无法找到五个部分" << std::endl;
//             abort();
//         }
//         slash_count++;
//     }

//     // 截取从找到的斜杠位置到末尾的子字符串
//     std::string result = input_file.substr(pos + 1);
//     return result;
// }

// // 提取单个文件名的函数
// std::string extract_graph_file(const std::string& input_file, int num) {
//     size_t pos = input_file.size();
//     int slash_count = 0;

//     while (slash_count < num) {
//         pos = input_file.rfind('/', pos - 1); // 向左查找下一个斜杠
//         if (pos == std::string::npos) {
//             std::cerr << "Error: 路径中斜杠数量不足, 无法找到指定的部分" << std::endl;
//             std::abort();
//         }
//         slash_count++;
//     }

//     return input_file.substr(pos + 1); // 提取从找到的斜杠到末尾的部分
// }


std::string extract_graph_file(std::string input_file, int num) {
    // 从右边开始查找第五个斜杠的位置
    size_t pos = input_file.size();
    int slash_count = 0;
    
    while (slash_count < num) {
        pos = input_file.rfind('/', pos - 1); // 向左查找下一个斜杠
        if (pos == std::string::npos) {
            LOG() << "Error: 路径中斜杠数量不足, 无法找到五个部分" << std::endl;
            abort();
        }
        slash_count++;
    }

    // 截取从找到的斜杠位置到末尾的子字符串
    std::string result = input_file.substr(pos + 1);
    return result;
}

// // 修改主解析逻辑
// std::vector<std::string> parse_query_graphs(const std::string& input_query_graphs, int num=5) {
//     // 分割多个文件路径
//     std::vector<std::string> query_files = split(input_query_graphs, ',');

//     // 逐个处理每个文件路径
//     for (const std::string& file_path : query_files) {
//         std::string exact_qgraph_file = extract_graph_file(file_path, num);
//     }

//     return query_files;
// }



void split_Q_test(MatchingCommand& command) {
    std::string input_query_graph_file = command.getQueryGraphFilePath();
    std::string input_data_graph_file = command.getDataGraphFilePath();
    std::string input_max_embedding_num = command.getMaximumEmbeddingNum();
    std::string input_time_limit = command.getTimeLimit();
    std::string input_distribution_file_path = command.getDistributionFilePath();
    std::string input_output_file = command.getOutputFile();
    std::string input_nums_threads = command.getThreadNumbers();
    // std::string input_type_split = command.getSplitType();
    // std::string input_type_schedule = command.getScheduleType();
    std::string back_method_types = command.getBackMethodTypes();
    std::string input_QorCandi_split = command.getQorCandiType();

    std::string input_Qpattern_split = command.getQpatternType();
    std::string input_join_paradigm = command.getJoinMethod();

    std::set<std::string> incompatible_set{};
    std::set<std::string> wanted_set{};
    std::set<std::string> exclued_filer_set{};
    std::set<std::string> exclued_order_set{};
    std::set<std::string> exclued_engine_set{};
    auto start = std::chrono::steady_clock::now();

    Graph* data_graph = new Graph(true);
    data_graph->loadGraphFromFile(input_data_graph_file);

    Graph* query_graph = new Graph(true);
    query_graph->loadGraphFromFile(input_query_graph_file);
    query_graph->buildCoreTable();

    auto end = std::chrono::steady_clock::now();
    int64_t load_graphs_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // std::cout << "back_method_types : " << back_method_types << std::endl;

    std::vector<int64_t> nums_threads;
    std::string token;
    std::stringstream tmpss_threads(input_nums_threads);
    // 直接按 delimiter 分割
    while (std::getline(tmpss_threads, token, ',')) {
        nums_threads.push_back(std::stoi(token));
    }

    // for (auto& num_threads : nums_threads) {
    //     std::cout << "Parsed num threads: " << num_threads << std::endl;
    // }

    std::vector<std::tuple<std::string, std::string, std::string>> backMethods;
    std::stringstream ss(back_method_types);
    std::string method;

    // 使用逗号分割字符串并将每个方法解析为 tuple 存储
    while (std::getline(ss, method, ',')) {
        // std::cout << "method: " << method << std::endl;
        std::stringstream methodStream(method);
        std::string filter_type, order_type, engine_type;

        // 使用 "-" 拆解每个 method 字符串
        std::getline(methodStream, filter_type, '_');
        std::getline(methodStream, order_type, '_');
        std::getline(methodStream, engine_type, '_');

        // 将拆解后的类型存入 tuple
        backMethods.push_back(std::make_tuple(filter_type, order_type, engine_type));
    }

    // // 打印解析后的结果
    // std::cout << "Parsed back methods:" << std::endl;
    // for (const auto& method_type : backMethods) {
    //     // 提取元组中的值
    //     std::string filter_type = std::get<0>(method_type);
    //     std::string order_type = std::get<1>(method_type);
    //     std::string engine_type = std::get<2>(method_type);

    //     // 输出每个方法的拆解结果
    //     std::cout << "Filter: " << filter_type
    //               << ", Order: " << order_type
    //               << ", Engine: " << engine_type << std::endl;
    // }

/**
 * Start queries.
 */
// 这三种是spli_candidate的策略

    // std::cout << "extract : " << extract_graph_file(input_query_graph_file, 1) << std::endl;

    // LOG() << "Output file: " << input_output_file << std::endl;

    Log::setLogFilePath(input_output_file);
    std::string exact_qgraph_file = extract_graph_file(input_query_graph_file,4);
    std::string exact_dgraph_file = extract_graph_file(input_data_graph_file, 3);

    // LOG() << "Start the query" << std::endl;

for (auto& num_threads : nums_threads) {

    // std::cout << "num_threads: " << num_threads << std::endl;

for (auto& method_type : backMethods) {

    std::string filter_type = std::get<0>(method_type);
    std::string order_type = std::get<1>(method_type);
    std::string engine_type = std::get<2>(method_type);

    // std::cout << "filter_type: " << filter_type << std::endl;
    // std::cout << "order_type: " << order_type << std::endl;
    // std::cout << "engine_type: " << engine_type << std::endl;

    /********************************** add method constarin *************************************/
    std::string sentence = filter_type + order_type + engine_type;
    bool flag_run = true;
    do {
        if (incompatible_set.find(sentence) != incompatible_set.end() // incompatible methods
            || (engine_type == "RM" && engine_type != order_type)) {  // RM engine workes well only with order RM
            LOG() << "flag_run = false  1;" << std::endl;
            flag_run = false;
            break;
        }
        if (wanted_set.find(sentence) != wanted_set.end()) break;  // want this combination
        if (exclued_filer_set.find(filter_type) != exclued_filer_set.end() // excluded filter | order | enum methods
            || exclued_order_set.find(order_type) != exclued_order_set.end()
            || exclued_engine_set.find(engine_type) != exclued_engine_set.end()) {
            flag_run = false;
            LOG() << "flag_run = false  2;" << std::endl;
            break;
        }
    } while(0);
    if (flag_run == false) {
        LOG() << "Error: " << sentence << " is not supported." << std::endl;
        abort();
        continue;
    }

    FAILING_SET_FLAG = 0;
    FROZEN_SET_FLAG = 0;

    if (engine_type == "DPiso") {
        FAILING_SET_FLAG = 1;
    }
    if (engine_type == "CIRCINUS") {
        FROZEN_SET_FLAG = 1;
    }

    if (engine_type == "GQL" || engine_type == "CECI")  {
        USING_CANDIDATE_INDEX_FLAG = 0;
    } else {
        USING_CANDIDATE_INDEX_FLAG = 1;
    }

    // LOG() << "USING_CANDIDATE_INDEX_FLAG: " << USING_CANDIDATE_INDEX_FLAG << std::endl;

    LOG() << "Start the query" << std::endl;
    LOG() << "Input Query Graph File: " << input_query_graph_file << std::endl;
    LOG() << "Input Data Graph File: " << exact_dgraph_file << std::endl;
    LOG() << "Maximum Embedding Number: " << input_max_embedding_num << std::endl;
    LOG() << "Time Limit: " << input_time_limit << std::endl;
    LOG() << "Num Threads: " << num_threads << std::endl;
    LOG() << "QuerySplit Pattern: " << input_Qpattern_split << std::endl;
    LOG() << "Join Paradigm: " << input_join_paradigm << std::endl;
    LOG() << "Filter Type: " << filter_type << std::endl;
    LOG() << "Order Type: " << order_type << std::endl;
    LOG() << "Engine Type: " << engine_type << std::endl;

    // exit(0);
    // LOG() << "-----" << std::endl;
    // LOG() << "Filter candidates..." << std::endl;

    int64_t time_limit; // 300s by default
    sscanf(input_time_limit.c_str(), "%ld", &time_limit); // second
    time_limit = time_limit * 1000 * 1000 * 1000;
    time_limit += intTimer::getClockNano();
    int64_t output_limit = 0;
    // size_t embedding_count = 0;
    if (input_max_embedding_num == "MAX") {
        output_limit = std::numeric_limits<int64_t>::max();
    }
    else {
        sscanf(input_max_embedding_num.c_str(), "%zu", &output_limit);
    }
    LOG() << "Output Limit: " << output_limit << std::endl;

    FilterVertices::time_limit = time_limit;

    start = std::chrono::steady_clock::now();

    ui** candidates = nullptr;
    ui* candidates_count = nullptr;
    ui* tso_order = nullptr;
    TreeNode* tso_tree = nullptr;
    ui* cfl_order = nullptr;
    TreeNode* cfl_tree = nullptr;
    ui* dpiso_order = nullptr;
    TreeNode* dpiso_tree = nullptr;
    TreeNode* ceci_tree = nullptr;
    ui* ceci_order = nullptr;
    std::vector<std::unordered_map<VertexID, std::vector<VertexID >>> TE_Candidates;
    std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> NTE_Candidates;
    if (filter_type == "LDF") {
        FilterVertices::LDFFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "NLF") {
        FilterVertices::NLFFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "GQL") {
        FilterVertices::GQLFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "TSO") {
        FilterVertices::TSOFilter(data_graph, query_graph, candidates, candidates_count, tso_order, tso_tree);
    } else if (filter_type == "CFL") {
        cfl_order = new ui[query_graph->getVerticesCount()];
        cfl_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::CFLFilter(data_graph, query_graph, candidates, candidates_count, cfl_order, cfl_tree);
        
        // std::ostringstream oss;
        // oss << "log cfl_order" << std::endl;        
        // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
        //     oss << cfl_order[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
    } else if (filter_type == "DPiso") {
        dpiso_order = new ui[query_graph->getVerticesCount()];
        dpiso_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::DPisoFilter(data_graph, query_graph, candidates, candidates_count, dpiso_order, dpiso_tree);
    } else if (filter_type == "CECI") {
        ceci_order = new ui[query_graph->getVerticesCount()];
        ceci_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::CECIFilter(data_graph, query_graph, candidates, candidates_count, ceci_order, ceci_tree, TE_Candidates, NTE_Candidates);
    }  else {
        LOG() << "The specified filter type '" << filter_type << "' is not supported." << std::endl;
        exit(-1);
    }

    // Sort the candidates to support the set intersections
    if (filter_type != "CECI")
        FilterVertices::sortCandidates(candidates, candidates_count, query_graph->getVerticesCount());

    end = std::chrono::steady_clock::now();
    int64_t filter_vertices_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    if (FilterVertices::checkOverTime()) {
        LOG() << "Timeout 1." << std::endl;
        LOG() << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
        LOG() << "Build table time (seconds): " << 0 << std::endl;
        LOG() << "Filter vertices time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
        LOG() << "Split Qeury Pattern Time: " << 0 << std::endl;
        LOG() << "Generate query plan time (seconds): " << 0 << std::endl;
        LOG() << "Enumerate time (seconds): " << 0 << std::endl;
        LOG() << "Preprocessing time (seconds): " << 0 << std::endl;
        LOG() << "Join time (seconds): " << 0 << std::endl;
        LOG() << "Total time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
        LOG() << "Memory cost (MB): " << 0 << std::endl;
        LOG() << "#Embeddings: " << 0 << std::endl;
        LOG() << "Call Count: " << 0 << std::endl;
        LOG() << "Per Call Time (nanoseconds): " << 0 << std::endl;
        LOG() << "Overtime: " << 1 << std::endl; // overtime = 1 means timeout
        LOG() << "End the query" << std::endl;

        delete[] tso_order;
        delete[] tso_tree;
        delete[] cfl_order;
        delete[] cfl_tree;
        delete[] dpiso_order;
        delete[] dpiso_tree;
        delete[] ceci_order;
        delete[] ceci_tree;
        // delete[] matching_order;

        candidates = nullptr;
        candidates_count = nullptr;
        continue;
    }

    // Compute the candidates false positive ratio.
#ifdef OPTIMAL_CANDIDATES
    std::vector<ui> optimal_candidates_count;
    int64_t avg_false_positive_ratio = FilterVertices::computeCandidatesFalsePositiveRatio(data_graph, query_graph, candidates,
                                                                                          candidates_count, optimal_candidates_count);
    FilterVertices::printCandidatesInfo(query_graph, candidates_count, optimal_candidates_count);
#endif
    LOG() << "-----" << std::endl;
    LOG() << "Build indices..." << std::endl;

    start = std::chrono::steady_clock::now();

    Edges ***edge_matrix = nullptr;
    if (filter_type != "CECI") {
        edge_matrix = new Edges **[query_graph->getVerticesCount()];
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            edge_matrix[i] = new Edges *[query_graph->getVerticesCount()];
        }

        BuildTable::buildTables(data_graph, query_graph, candidates, candidates_count, edge_matrix);
    }

    end = std::chrono::steady_clock::now();
    int64_t build_table_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    size_t memory_cost_in_bytes = 0;
    if (filter_type != "CECI") {
        memory_cost_in_bytes = BuildTable::computeMemoryCostInBytes(query_graph, candidates_count, edge_matrix);
        // BuildTable::printTableCardinality(query_graph, edge_matrix);
    }
    else {
        memory_cost_in_bytes = BuildTable::computeMemoryCostInBytes(query_graph, candidates_count, ceci_order, ceci_tree,
                TE_Candidates, NTE_Candidates);
        // BuildTable::printTableCardinality(query_graph, ceci_tree, ceci_order, TE_Candidates, NTE_Candidates);
    }

    LOG() << "-----" << std::endl;
    LOG() << "Generate a matching order..." << std::endl;

    ui* matching_order = nullptr;
    ui* pivots = nullptr;
    ui** weight_array = nullptr;

    // size_t order_num = 0;

    std::vector<std::vector<ui>> spectrum;

    TaskSlot::g_args = new GLOBALARGS(data_graph, query_graph, edge_matrix,
        candidates, candidates_count, output_limit, time_limit);

    JoinParadigm join_paradigm;
    if (input_join_paradigm == "LEFTDEEP") {
        join_paradigm = JoinParadigm::LEFTDEEP;
    } else if (input_join_paradigm == "BUSHY") {
        join_paradigm = JoinParadigm::BUSHY;
    }

    SplitPattern split_pattern;
    if (input_Qpattern_split == "SKETCHTREE") {
        split_pattern = SplitPattern::HYBRIDPATH_RANDOM;
    } else if (input_Qpattern_split == "STAR") {
        split_pattern = SplitPattern::STAR;
    } else if (input_Qpattern_split == "TWINTWIG") {
        split_pattern = SplitPattern::TWINTWIG;   
    } else if (input_Qpattern_split == "SEED") {
        split_pattern = SplitPattern::SEED;
    }

    if (input_QorCandi_split == "Q") {
        TaskSlot::g_args->splitMode = SPLITMODE::Q;
    } else if (input_QorCandi_split == "C"){
        TaskSlot::g_args->splitMode = SPLITMODE::C;
        if (num_threads == 1) {
            TaskSlot::g_args->splitMode = SPLITMODE::NOSPLIT;
            // LOG() << "SplitMode: NOSPLIT" << std::endl;
        }
    } else {
        LOG() << "Error: The split mode is not defined." << std::endl;
        abort();
    }

    if (order_type == "DPiso" || order_type == "CIRCINUS") {
        TaskSlot::g_args->tree = dpiso_tree;
        TaskSlot::g_args->tree_order = dpiso_order;
    } else if (order_type == "CECI") {
        TaskSlot::g_args->tree = ceci_tree;
        TaskSlot::g_args->tree_order = ceci_order;
        // std::ostringstream oss;
        // oss << "log g_args->tree_order: " << std::endl;
        // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
        //     oss << ceci_order[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
    } else if (order_type == "CFL") {
        TaskSlot::g_args->tree = cfl_tree;
        TaskSlot::g_args->tree_order = cfl_order;
    } else if (order_type == "GQL") {

    } else {
        // 暂时把GQL的order给ban了，因为要tree和bn
        LOG() << "The specified order type '" << order_type << "' is not supported." << std::endl;
        abort();
    }

    if (engine_type == "CECI") {
        TaskSlot::g_args->TE_Candidates = TE_Candidates;
        TaskSlot::g_args->NTE_Candidates = NTE_Candidates;
    }

    bool needEdgeMatrix = true;
    // if (engine_type == "CECI" || engine_type == "GQL") {
    //     needEdgeMatrix = false;
    // }
    bool needCandidates = true;
    TaskSlot::g_args->needCandidates = needCandidates;
    TaskSlot::g_args->needEdgeMatrix = needEdgeMatrix;
    if (needCandidates) {
        // TaskSlot::g_args->printCandidates();
    }
    if (needEdgeMatrix) {
        // TaskSlot::g_args->printEdgeMatrix();
    }

    auto split_start = std::chrono::steady_clock::now();
    SplitGraph* multigraphs = 
        new SplitGraph(data_graph, query_graph, join_paradigm, split_pattern, TaskSlot::g_args->splitMode, time_limit);
    auto split_end = std::chrono::steady_clock::now();
    int64_t split_query_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(split_end - split_start).count();

    if (FilterVertices::checkOverTime()) {
        int64_t total_time_in_ns = filter_vertices_time_in_ns + build_table_time_in_ns + split_query_time_in_ns;
        LOG() << "Timeout 2." << std::endl;
        LOG() << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
        LOG() << "Build table time (seconds): " << 0 << std::endl;
        LOG() << "Filter vertices time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
        LOG() << "Split Qeury Pattern Time: " << NANOSECTOSEC(split_query_time_in_ns) << std::endl;
        LOG() << "Generate query plan time (seconds): " << 0 << std::endl;
        LOG() << "Enumerate time (seconds): " << 0 << std::endl;
        LOG() << "Preprocessing time (seconds): " << 0 << std::endl;
        LOG() << "Join time (seconds): " << 0 << std::endl;
        LOG() << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
        LOG() << "Memory cost (MB): " << 0 << std::endl;
        LOG() << "#Embeddings: " << 0 << std::endl;
        LOG() << "Call Count: " << 0 << std::endl;
        LOG() << "Per Call Time (nanoseconds): " << 0 << std::endl;
        LOG() << "Overtime: " << 1 << std::endl; // overtime = 1 means timeout
        LOG() << "End the query" << std::endl;

        delete[] tso_order;
        delete[] tso_tree;
        delete[] cfl_order;
        delete[] cfl_tree;
        delete[] dpiso_order;
        delete[] dpiso_tree;
        delete[] ceci_order;
        delete[] ceci_tree;
        // delete[] matching_order;
        delete[] pivots;
        if (weight_array != nullptr) {
            for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
                delete[] weight_array[i];
            }
            delete[] weight_array;
        }

        delete TaskSlot::g_args;
        delete multigraphs;
        edge_matrix = nullptr;
        candidates = nullptr;
        candidates_count = nullptr;
        matching_order = nullptr;

        continue;
    }

    // multigraphs->printSketchTree();
    // 用SplitGraph里面的InducedSubgraphs来初始化unitArgsVec
    // LOG() << multigraphs;
    start = std::chrono::steady_clock::now();

    // LOG() << "subgraph count == " << multigraphs->getSubgraphCount() << std::endl;
    UnitArgs** unitArgsVec = new UnitArgs*[multigraphs->getSubgraphCount()];
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        InducedSubgraph* q_graph = multigraphs->getSubgraphs()[id_unit];
        // LOG() << "subgraph " << id_unit << " vertices count == " << q_graph->subgraph->getVerticesCount() << std::endl;
        unitArgsVec[id_unit] = new UnitArgs(q_graph, output_limit);
        if (needCandidates) {
            unitArgsVec[id_unit]->mapUidCandidates(TaskSlot::g_args);
        }
        if (needEdgeMatrix) {
            unitArgsVec[id_unit]->mapUidEdgeMatrix(TaskSlot::g_args);
        }
        // if (engine_type == "CECI") {
        //     unitArgsVec[id_unit]->mapUidCandidatesCECI(TaskSlot::g_args);
        // }
    }

    TaskSlot::g_args->unitArgsVec = unitArgsVec;
    TaskSlot::g_args->count_unit = multigraphs->getSubgraphCount();

    bool needPivot = false;
    bool needBN = true;
    // if (order_type == "EXPLORE" || order_type == "DPiso") {
    //     needPivot = true;
    // }    
    if (engine_type == "EXPLORE" || engine_type == "DPiso") {
        needPivot = true;
    }
    if (engine_type == "CECI") {
        needBN = false;
    }

    if (order_type == "DPiso") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            // u_args->query_graph->printGraph();
            // 每个子图的dpiso_tree都要建一下
            // LOG() << "Generate DPiso tree for unit " << id_unit << std::endl;
            // LOG() << "print unit graph : " << std::endl;
            // q_graph->printGraph();
            /*
                generateDPisoFilterPlan 生成的是 tree_order
                generateDPisoQueryPlan 生成的是 matching_order
             */
            GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, u_args);
            // LOG() << "Generate DPiso order for unit " << id_unit << std::endl;
            GenerateQueryPlan::generateDPisoQueryPlan(q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            // std::ostringstream oss;
            // oss << "unit_order : " << std::endl;
            // for (ui i = 0; i < u_args->vertices_count; ++i) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();

            // NOTE: LFTJ这种不用pivot的，没法子做这个检查
            // if (order_type != "Spectrum") {
            //     GenerateQueryPlan::checkQueryPlanCorrectness(q_graph, u_args->order, u_args->pivots);
            //     // GenerateQueryPlan::printSimplifiedQueryPlan(query_graph, matching_order);
            // } else {
            //     LOG() << "Generate " << spectrum.size() << " matching orders." << std::endl;
            // }
        }

        // // original
        // // 这里是因为filter的时候可能是LDF之类的，还没生成这个tree
        // if (dpiso_tree == nullptr) {
        //     GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, query_graph, dpiso_tree, dpiso_order);
        // }
        // GenerateQueryPlan::generateDSPisoQueryPlan(query_graph, edge_matrix, matching_order, pivots, dpiso_tree, dpiso_order,
        //                                             candidates_count, weight_array);
    } else if (order_type == "CIRCINUS") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, u_args);
            // LOG() << "Generate DPiso order for unit " << id_unit << std::endl;
            GenerateQueryPlan::generateDPisoQueryPlan(q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            u_args->generateCircinusLayer();
        }
    } else if (order_type == "CECI") {
        LOG() << "Error: split_Q not support CECI" << std::endl;
        abort();
    } else if (order_type == "GQL") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            GenerateQueryPlan::generateGQLQueryPlan(data_graph, q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            // std::ostringstream oss;
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < q_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit bn" << std::endl;
            // for (ui i = 0; i < q_graph->getVerticesCount(); i++) {
            //     oss << "bn_count[" << i << "] = " << u_args->bn_count[i] << std::endl;
            //     for (ui j = 0; j < u_args->bn_count[i]; j++) {
            //         oss << u_args->bn[i][j] << ", ";
            //     }
            //     oss << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
        }

    } else if (order_type == "CFL") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            
            GenerateFilteringPlan::generateCFLFilterPlan(data_graph, u_args);
            GenerateQueryPlan::generateCFLQueryPlan(q_graph, u_args);
            EvaluateQuery::generateBNWithPivot(u_args);
            // std::ostringstream oss;
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < q_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit bn" << std::endl;
            // for (ui i = 0; i < q_graph->getVerticesCount(); i++) {
            //     oss << "bn_count[" << i << "] = " << u_args->bn_count[i] << std::endl;
            //     for (ui j = 0; j < u_args->bn_count[i]; j++) {
            //         oss << u_args->bn[i][j] << ", ";
            //     }
            //     oss << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
        }
    } else {
        LOG() << "The specified order type '" << order_type << "' is not supported." << std::endl;
        abort();
    }

    /*
        print order, candidates, edge_matrix of subgraphs
     */
    // for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
    //     UnitArgs* u_args = unitArgsVec[id_unit];
    //     for (ui i = 0; i < unitArgsVec[id_unit]->query_graph->subgraph->getVerticesCount(); i++) {
    //         LOG() << "order : " << unitArgsVec[id_unit]->order[i] << std::endl;
    //     }
    //     u_args->printCandidates();
    //     u_args->printEdgeMatrix();
    // }

    end = std::chrono::steady_clock::now();
    int64_t generate_query_plan_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    LOG() << "-----" << std::endl;
    LOG() << "Enumerate..." << std::endl;


#if ENABLE_QFLITER == 1
    EvaluateQuery::qfliter_bsr_graph_ = BuildTable::qfliter_bsr_graph_;
#endif

    /**
     * Engine Start here
     */
    // TODO: change all pointer to smart pointer
    // shallow copy the global info


    start = std::chrono::steady_clock::now();
    // split_Q是按照shixuan和default来的
    int64_t queue_capacity = num_threads * 2;
    TaskPool* taskPool = new TaskPool(num_threads, queue_capacity);

    // TODO: split_type and schedule_type
    if (engine_type == "LFTJ") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::LFTJ(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "GQL") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreGraphQLStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "EXPLORE") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreGraph(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "DPiso") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreDPisoStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "CECI") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreCECIStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            unitArgsVec[id_unit]->tree = nullptr;
            unitArgsVec[id_unit]->tree_order = nullptr;
        }
    } else if (engine_type == "CIRCINUS") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreCircinusStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else {
        LOG() << "Not supported engine type: " << engine_type << std::endl;
    }

    end = std::chrono::steady_clock::now();
    int64_t enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    if (FilterVertices::checkOverTime()) {
        int64_t total_time_in_ns = filter_vertices_time_in_ns + build_table_time_in_ns + split_query_time_in_ns + generate_query_plan_time_in_ns + enumeration_time_in_ns;
        LOG() << "Timeout 3." << std::endl;
        LOG() << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
        LOG() << "Build table time (seconds): " << 0 << std::endl;
        LOG() << "Filter vertices time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
        LOG() << "Split Qeury Pattern Time: " << NANOSECTOSEC(split_query_time_in_ns) << std::endl;
        LOG() << "Generate query plan time (seconds): " << NANOSECTOSEC(generate_query_plan_time_in_ns) << std::endl;
        LOG() << "Enumerate time (seconds): " << NANOSECTOSEC(enumeration_time_in_ns) << std::endl;
        LOG() << "Preprocessing time (seconds): " << 0 << std::endl;
        LOG() << "Join time (seconds): " << 0 << std::endl;
        LOG() << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
        LOG() << "Memory cost (MB): " << 0 << std::endl;
        LOG() << "#Embeddings: " << 0 << std::endl;
        LOG() << "Call Count: " << 0 << std::endl;
        LOG() << "Per Call Time (nanoseconds): " << 0 << std::endl;
        LOG() << "Overtime: " << 1 << std::endl; // overtime = 1 means timeout
        LOG() << "End the query" << std::endl;

        delete[] tso_order;
        delete[] tso_tree;
        delete[] cfl_order;
        delete[] cfl_tree;
        delete[] dpiso_order;
        delete[] dpiso_tree;
        delete[] ceci_order;
        delete[] ceci_tree;
        // delete[] matching_order;
        delete[] pivots;
        if (weight_array != nullptr) {
            for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
                delete[] weight_array[i];
            }
            delete[] weight_array;
        }

        delete taskPool;
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            delete unitArgsVec[id_unit];
        }
        delete[] unitArgsVec;
        delete TaskSlot::g_args;
        delete multigraphs;
        edge_matrix = nullptr;
        candidates = nullptr;
        candidates_count = nullptr;
        matching_order = nullptr;

        continue;
    } else {
        // LOG() << "not over time_limit" << std::endl;
    }

    // LOG() << "partial_matches sizes : " << std::endl;
    // for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
    //     LOG() << "unit " << id_unit << ": " << unitArgsVec[id_unit]->unit_partial_match->data.size() / unitArgsVec[id_unit]->unit_partial_match->length << std::endl;
    // }

    // multigraphs->printPaths();

    start = std::chrono::steady_clock::now();
    JoinCollect* jc;
    if (split_pattern == SplitPattern::HYBRIDPATH_RANDOM) {
        jc = new JoinCollect(data_graph, query_graph, multigraphs->jtree, multigraphs->getSubgraphCount(), TaskSlot::g_args->time_limit, taskPool);
    } else {
        jc = new JoinCollect(data_graph, query_graph, multigraphs->jseq, multigraphs->getSubgraphCount(), TaskSlot::g_args->time_limit, taskPool);
    }
    jc->unit_count = multigraphs->getSubgraphCount();
    // LOG
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        jc->partial_matches[id_unit] = unitArgsVec[id_unit]->unit_partial_match;
        // LOG() << "unit " << id_unit << " partial match size: "
        //       << unitArgsVec[id_unit]->unit_partial_match->data.size() / int64_t(unitArgsVec[id_unit]->unit_partial_match->length)
        //       << std::endl;
    }

    PartialMatch* res;
    if (split_pattern == SplitPattern::HYBRIDPATH_RANDOM) {
        res = jc->recur_collect(multigraphs->jtree->jointree_root);
    } else {
        res = jc->sequence_collect();
    }

    if (TaskSlot::g_args->splitMode == SPLITMODE::Q) {
        if (res) {
            TaskSlot::g_args->embedding_count = res->size;
        } else {
            // LOG() << "nullptr joined embedding count : 0, not finished" << std::endl;
            TaskSlot::g_args->embedding_count = 0;
        }
    }
    if (FilterVertices::checkOverTime()) {
        TaskSlot::g_args->overtime = true;
        // TaskSlot::g_args->embedding_count = 0;
    }
    delete jc;
    delete res;

    end = std::chrono::steady_clock::now();
    int64_t join_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

#ifdef DISTRIBUTION
    std::ofstream outfile (input_distribution_file_path , std::ofstream::binary);
    outfile.write((char*)EvaluateQuery::distribution_count_, sizeof(size_t) * data_graph->getVerticesCount());
    delete[] EvaluateQuery::distribution_count_;
#endif

    /**
     * End.
     */
    // embedding_count = TaskSlot::g_args->embedding_count;
    // call_count = TaskSlot::g_args->call_count;
    // LOG() << "--------------------------------------------------------------------" << std::endl;
    generate_query_plan_time_in_ns -= split_query_time_in_ns;
    int64_t preprocessing_time_in_ns = filter_vertices_time_in_ns + build_table_time_in_ns + generate_query_plan_time_in_ns;
    int64_t total_time_in_ns = preprocessing_time_in_ns + enumeration_time_in_ns + split_query_time_in_ns + join_time_in_ns;

    TaskSlot::g_args->load_graphs_time_in_ns = load_graphs_time_in_ns;
    TaskSlot::g_args->filter_vertices_time_in_ns = filter_vertices_time_in_ns;
    TaskSlot::g_args->split_query_time_in_ns = split_query_time_in_ns;
    TaskSlot::g_args->build_table_time_in_ns = build_table_time_in_ns;
    TaskSlot::g_args->generate_query_plan_time_in_ns = generate_query_plan_time_in_ns;
    TaskSlot::g_args->enumeration_time_in_ns = enumeration_time_in_ns;
    TaskSlot::g_args->preprocessing_time_in_ns = preprocessing_time_in_ns;
    TaskSlot::g_args->join_time_in_ns = join_time_in_ns;
    TaskSlot::g_args->total_time_in_ns = total_time_in_ns;
    TaskSlot::g_args->memory_cost_in_bytes = memory_cost_in_bytes;

    TaskSlot::g_args->logGlobalStatistics();

    // delete query_graph;
    // delete data_graph;
    // destroy below resources in GLOBALARGS destructor



    // LOG() << "--------------------------------------------------------------------" << std::endl;
    // LOG() << "Release memories..." << std::endl;
    /**
     * Release the allocated memories.
     */
    // delete[] candidates_count;
    delete[] tso_order;
    delete[] tso_tree;
    delete[] cfl_order;
    delete[] cfl_tree;
    delete[] dpiso_order;
    delete[] dpiso_tree;
    delete[] ceci_order;
    delete[] ceci_tree;
    // delete[] matching_order;
    delete[] pivots;
    if (weight_array != nullptr) {
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            delete[] weight_array[i];
        }
        delete[] weight_array;
    }

    delete taskPool;
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        delete unitArgsVec[id_unit];
    }
    delete[] unitArgsVec;
    delete TaskSlot::g_args;
    delete multigraphs;
    edge_matrix = nullptr;
    candidates = nullptr;
    candidates_count = nullptr;
    matching_order = nullptr;
} // end of method loop
} // end of thread loop

    delete query_graph;
    query_graph = nullptr;

    delete data_graph;
    data_graph = nullptr;
}


void split_C_test(MatchingCommand& command) {
    std::string input_query_graph_file = command.getQueryGraphFilePath();
    std::string input_data_graph_file = command.getDataGraphFilePath();
    std::string input_max_embedding_num = command.getMaximumEmbeddingNum();
    std::string input_time_limit = command.getTimeLimit();
    std::string input_distribution_file_path = command.getDistributionFilePath();
    std::string input_output_file = command.getOutputFile();
    std::string input_nums_threads = command.getThreadNumbers();
    std::string input_type_split = command.getSplitType();
    std::string input_type_schedule = command.getScheduleType();
    std::string back_method_types = command.getBackMethodTypes();
    std::string input_QorCandi_split = command.getQorCandiType();

    std::string input_Qpattern_split = command.getQpatternType();
    std::string input_join_paradigm = command.getJoinMethod();

    // std::cout << "log all inputs" << std::endl;
    // std::cout << "input_query_graph_file : " << input_query_graph_file << std::endl;
    // std::cout << "input_data_graph_file : " << input_data_graph_file << std::endl;
    // std::cout << "input_max_embedding_num : " << input_max_embedding_num << std::endl;
    // std::cout << "input_time_limit : " << input_time_limit << std::endl;
    // std::cout << "input_distribution_file_path : " << input_distribution_file_path << std::endl;
    // std::cout << "input_output_dir : " << input_output_dir << std::endl;
    // std::cout << "input_nums_threads : " << input_nums_threads << std::endl;
    // std::cout << "input_type_split : " << input_type_split << std::endl;
    // std::cout << "input_type_schedule : " << input_type_schedule << std::endl;
    // std::cout << "back_method_types : " << back_method_types << std::endl;
    // std::cout << "input_QorCandi_split : " << input_QorCandi_split << std::endl;
    // std::cout << "input_Qpattern_split : " << input_Qpattern_split << std::endl;
    // std::cout << "input_join_paradigm : " << input_join_paradigm << std::endl;

    std::set<std::string> incompatible_set{};
    std::set<std::string> wanted_set{};
    std::set<std::string> exclued_filer_set{};
    std::set<std::string> exclued_order_set{};
    std::set<std::string> exclued_engine_set{};
    auto start = std::chrono::steady_clock::now();

    Graph* data_graph = new Graph(true);
    data_graph->loadGraphFromFile(input_data_graph_file);

    Graph* query_graph = new Graph(true);
    query_graph->loadGraphFromFile(input_query_graph_file);
    query_graph->buildCoreTable();

    auto end = std::chrono::steady_clock::now();
    int64_t load_graphs_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // std::cout << "back_method_types : " << back_method_types << std::endl;

    std::vector<int64_t> nums_threads;
    std::string token;
    std::stringstream tmpss_threads(input_nums_threads);
    // 直接按 delimiter 分割
    while (std::getline(tmpss_threads, token, ',')) {
        nums_threads.push_back(std::stoi(token));
    }

    // for (auto& num_threads : nums_threads) {
    //     std::cout << "Parsed num threads: " << num_threads << std::endl;
    // }

    std::vector<std::tuple<std::string, std::string, std::string>> backMethods;
    std::stringstream ss(back_method_types);
    std::string method;

    // 使用逗号分割字符串并将每个方法解析为 tuple 存储
    while (std::getline(ss, method, ',')) {
        // std::cout << "method: " << method << std::endl;
        std::stringstream methodStream(method);
        std::string filter_type, order_type, engine_type;

        // 使用 "-" 拆解每个 method 字符串
        std::getline(methodStream, filter_type, '_');
        std::getline(methodStream, order_type, '_');
        std::getline(methodStream, engine_type, '_');

        // 将拆解后的类型存入 tuple
        backMethods.push_back(std::make_tuple(filter_type, order_type, engine_type));
    }

    // // 打印解析后的结果
    // std::cout << "Parsed back methods:" << std::endl;
    // for (const auto& method_type : backMethods) {
    //     // 提取元组中的值
    //     std::string filter_type = std::get<0>(method_type);
    //     std::string order_type = std::get<1>(method_type);
    //     std::string engine_type = std::get<2>(method_type);

    //     // 输出每个方法的拆解结果
    //     std::cout << "Filter: " << filter_type
    //               << ", Order: " << order_type
    //               << ", Engine: " << engine_type << std::endl;
    // }

/**
 * Start queries.
 */
// 这三种是spli_candidate的策略

    // std::cout << "extract : " << extract_graph_file(input_query_graph_file, 1) << std::endl;

    // LOG() << "Output file: " << input_output_file << std::endl;

    Log::setLogFilePath(input_output_file);
    std::string exact_qgraph_file = extract_graph_file(input_query_graph_file,4);
    std::string exact_dgraph_file = extract_graph_file(input_data_graph_file, 3);

    // LOG() << "Start the query" << std::endl;

for (auto& num_threads : nums_threads) {

    // std::cout << "num_threads: " << num_threads << std::endl;

for (auto& method_type : backMethods) {

    std::string filter_type = std::get<0>(method_type);
    std::string order_type = std::get<1>(method_type);
    std::string engine_type = std::get<2>(method_type);

    // std::cout << "filter_type: " << filter_type << std::endl;
    // std::cout << "order_type: " << order_type << std::endl;
    // std::cout << "engine_type: " << engine_type << std::endl;

    /********************************** add method constarin *************************************/
    std::string sentence = filter_type + order_type + engine_type;
    bool flag_run = true;
    do {
        if (incompatible_set.find(sentence) != incompatible_set.end() // incompatible methods
            || (engine_type == "RM" && engine_type != order_type)) {  // RM engine workes well only with order RM
            LOG() << "flag_run = false  1;" << std::endl;
            flag_run = false;
            break;
        }
        if (wanted_set.find(sentence) != wanted_set.end()) break;  // want this combination
        if (exclued_filer_set.find(filter_type) != exclued_filer_set.end() // excluded filter | order | enum methods
            || exclued_order_set.find(order_type) != exclued_order_set.end()
            || exclued_engine_set.find(engine_type) != exclued_engine_set.end()) {
            flag_run = false;
            LOG() << "flag_run = false  2;" << std::endl;
            break;
        }
    } while(0);
    if (flag_run == false) {
        LOG() << "Error: " << sentence << " is not supported." << std::endl;
        abort();
        continue;
    }

    FAILING_SET_FLAG = 0;
    FROZEN_SET_FLAG = 0;

    if (engine_type == "DPiso") {
        FAILING_SET_FLAG = 1;
    }
    if (engine_type == "CIRCINUS") {
        FROZEN_SET_FLAG = 1;
    }

    if (engine_type == "GQL" || engine_type == "CECI")  {
        USING_CANDIDATE_INDEX_FLAG = 0;
    } else {
        USING_CANDIDATE_INDEX_FLAG = 1;
    }

    // LOG() << "USING_CANDIDATE_INDEX_FLAG: " << USING_CANDIDATE_INDEX_FLAG << std::endl;

    LOG() << "Start the query" << std::endl;
    LOG() << "Input Query Graph File: " << input_query_graph_file << std::endl;
    LOG() << "Input Data Graph File: " << exact_dgraph_file << std::endl;
    LOG() << "Maximum Embedding Number: " << input_max_embedding_num << std::endl;
    LOG() << "Time Limit: " << input_time_limit << std::endl;
    LOG() << "Num Threads: " << num_threads << std::endl;
    LOG() << "Split Type: " << input_type_split << std::endl;
    LOG() << "Schedule Type: " << input_type_schedule << std::endl;
    LOG() << "Filter Type: " << filter_type << std::endl;
    LOG() << "Order Type: " << order_type << std::endl;
    LOG() << "Engine Type: " << engine_type << std::endl;

    // exit(0);
    // LOG() << "-----" << std::endl;
    // LOG() << "Filter candidates..." << std::endl;

    int64_t time_limit; // 300s by default
    sscanf(input_time_limit.c_str(), "%ld", &time_limit); // second
    time_limit = time_limit * 1000 * 1000 * 1000;
    time_limit += intTimer::getClockNano();
    int64_t output_limit = 0;
    // size_t embedding_count = 0;
    if (input_max_embedding_num == "MAX") {
        output_limit = std::numeric_limits<int64_t>::max();
    }
    else {
        sscanf(input_max_embedding_num.c_str(), "%zu", &output_limit);
    }
    LOG() << "Output Limit: " << output_limit << std::endl;

    FilterVertices::time_limit = time_limit;

    start = std::chrono::steady_clock::now();

    ui** candidates = nullptr;
    ui* candidates_count = nullptr;
    ui* tso_order = nullptr;
    TreeNode* tso_tree = nullptr;
    ui* cfl_order = nullptr;
    TreeNode* cfl_tree = nullptr;
    ui* dpiso_order = nullptr;
    TreeNode* dpiso_tree = nullptr;
    TreeNode* ceci_tree = nullptr;
    ui* ceci_order = nullptr;
    std::vector<std::unordered_map<VertexID, std::vector<VertexID >>> TE_Candidates;
    std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> NTE_Candidates;
    if (filter_type == "LDF") {
        FilterVertices::LDFFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "NLF") {
        FilterVertices::NLFFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "GQL") {
        // TODO: 把这里替换成DPiso_filter之类的，构造出一个tree即可
        FilterVertices::GQLFilter(data_graph, query_graph, candidates, candidates_count);
    } else if (filter_type == "TSO") {
        FilterVertices::TSOFilter(data_graph, query_graph, candidates, candidates_count, tso_order, tso_tree);
    } else if (filter_type == "CFL") {
        cfl_order = new ui[query_graph->getVerticesCount()];
        cfl_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::CFLFilter(data_graph, query_graph, candidates, candidates_count, cfl_order, cfl_tree);
        
        // std::ostringstream oss;
        // oss << "log cfl_order" << std::endl;        
        // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
        //     oss << cfl_order[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
    } else if (filter_type == "DPiso") {
        dpiso_order = new ui[query_graph->getVerticesCount()];
        dpiso_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::DPisoFilter(data_graph, query_graph, candidates, candidates_count, dpiso_order, dpiso_tree);
    } else if (filter_type == "CECI") {
        ceci_order = new ui[query_graph->getVerticesCount()];
        ceci_tree = new TreeNode[query_graph->getVerticesCount()];
        FilterVertices::CECIFilter(data_graph, query_graph, candidates, candidates_count, ceci_order, ceci_tree, TE_Candidates, NTE_Candidates);
    }  else {
        LOG() << "The specified filter type '" << filter_type << "' is not supported." << std::endl;
        exit(-1);
    }

    // Sort the candidates to support the set intersections
    if (filter_type != "CECI")
        FilterVertices::sortCandidates(candidates, candidates_count, query_graph->getVerticesCount());

    end = std::chrono::steady_clock::now();
    int64_t filter_vertices_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    if (FilterVertices::checkOverTime()) {
        int64_t total_time_in_ns = filter_vertices_time_in_ns;
        LOG() << "Timeout 1." << std::endl;
        LOG() << "Load graphs time (seconds): " << NANOSECTOSEC(load_graphs_time_in_ns) << std::endl;
        LOG() << "Build table time (seconds): " << 0 << std::endl;
        LOG() << "Filter vertices time (seconds): " << NANOSECTOSEC(filter_vertices_time_in_ns) << std::endl;
        LOG() << "Generate query plan time (seconds): " << 0 << std::endl;
        LOG() << "Enumerate time (seconds): " << 0 << std::endl;
        LOG() << "Preprocessing time (seconds): " << 0 << std::endl;
        LOG() << "Total time (seconds): " << NANOSECTOSEC(total_time_in_ns) << std::endl;
        LOG() << "Memory cost (MB): " << 0 << std::endl;
        LOG() << "#Embeddings: " << 0 << std::endl;
        LOG() << "Call Count: " << 0 << std::endl;
        LOG() << "Per Call Time (nanoseconds): " << 0 << std::endl;
        LOG() << "Overtime: " << 1 << std::endl; // overtime = 1 means timeout
        LOG() << "End the query" << std::endl;

        delete[] tso_order;
        delete[] tso_tree;
        delete[] cfl_order;
        delete[] cfl_tree;
        delete[] dpiso_order;
        delete[] dpiso_tree;
        delete[] ceci_order;
        delete[] ceci_tree;
        // delete[] matching_order;

        candidates = nullptr;
        candidates_count = nullptr;
        continue;
    }

    // Compute the candidates false positive ratio.
#ifdef OPTIMAL_CANDIDATES
    std::vector<ui> optimal_candidates_count;
    int64_t avg_false_positive_ratio = FilterVertices::computeCandidatesFalsePositiveRatio(data_graph, query_graph, candidates,
                                                                                          candidates_count, optimal_candidates_count);
    FilterVertices::printCandidatesInfo(query_graph, candidates_count, optimal_candidates_count);
#endif
    LOG() << "-----" << std::endl;
    LOG() << "Build indices..." << std::endl;

    start = std::chrono::steady_clock::now();

    Edges ***edge_matrix = nullptr;
    if (filter_type != "CECI") {
        edge_matrix = new Edges **[query_graph->getVerticesCount()];
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            edge_matrix[i] = new Edges *[query_graph->getVerticesCount()];
        }

        BuildTable::buildTables(data_graph, query_graph, candidates, candidates_count, edge_matrix);
    }

    end = std::chrono::steady_clock::now();
    int64_t build_table_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    size_t memory_cost_in_bytes = 0;
    if (filter_type != "CECI") {
        memory_cost_in_bytes = BuildTable::computeMemoryCostInBytes(query_graph, candidates_count, edge_matrix);
        // BuildTable::printTableCardinality(query_graph, edge_matrix);
    }
    else {
        memory_cost_in_bytes = BuildTable::computeMemoryCostInBytes(query_graph, candidates_count, ceci_order, ceci_tree,
                TE_Candidates, NTE_Candidates);
        // BuildTable::printTableCardinality(query_graph, ceci_tree, ceci_order, TE_Candidates, NTE_Candidates);
    }

    LOG() << "-----" << std::endl;
    LOG() << "Generate a matching order..." << std::endl;

    start = std::chrono::steady_clock::now();

    ui* matching_order = nullptr;
    ui* pivots = nullptr;
    ui** weight_array = nullptr;

    // size_t order_num = 0;

    std::vector<std::vector<ui>> spectrum;

    TaskSlot::g_args = new GLOBALARGS(data_graph, query_graph, edge_matrix,
        candidates, candidates_count, output_limit, time_limit);

    if (num_threads == 1) {
        TaskSlot::g_args->splitMode = SPLITMODE::NOSPLIT;
        // LOG() << "SplitMode: NOSPLIT" << std::endl;
    } else {
        // Q 就默认是Q&C了
        if (input_QorCandi_split == "Q") {
            TaskSlot::g_args->splitMode = SPLITMODE::Q;
        } else if (input_QorCandi_split == "C"){
            TaskSlot::g_args->splitMode = SPLITMODE::C;
        // } else if (input_QorCandi_split == "Q_C") {
        //     TaskSlot::g_args->splitMode = SPLITMODE::Q_C;
        } else if (input_QorCandi_split == "NOSPLIT") {
            TaskSlot::g_args->splitMode = SPLITMODE::NOSPLIT;
        } else {
            LOG() << "Error: The split mode is not defined." << std::endl;
            abort();
        }
        // LOG() << "SplitMode: NEEDSPLIT" << std::endl;
    }

    if (order_type == "DPiso" || order_type == "CIRCINUS") {
        TaskSlot::g_args->tree = dpiso_tree;
        TaskSlot::g_args->tree_order = dpiso_order;
    } else if (order_type == "CECI") {
        TaskSlot::g_args->tree = ceci_tree;
        TaskSlot::g_args->tree_order = ceci_order;
        // std::ostringstream oss;
        // oss << "log g_args->tree_order: " << std::endl;
        // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
        //     oss << ceci_order[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();
    } else if (order_type == "CFL") {
        TaskSlot::g_args->tree = cfl_tree;
        TaskSlot::g_args->tree_order = cfl_order;
    } else if (order_type == "GQL") {

    } else {
        // 暂时把GQL的order给ban了，因为要tree和bn
        LOG() << "The specified order type '" << order_type << "' is not supported." << std::endl;
        abort();
    }

    if (engine_type == "CECI") {
        TaskSlot::g_args->TE_Candidates = TE_Candidates;
        TaskSlot::g_args->NTE_Candidates = NTE_Candidates;
    }


    bool needEdgeMatrix = true;
    // if (engine_type == "CECI" || engine_type == "GQL") {
    //     needEdgeMatrix = false;
    // }
    bool needCandidates = true;
    TaskSlot::g_args->needCandidates = needCandidates;
    TaskSlot::g_args->needEdgeMatrix = needEdgeMatrix;
    TaskSlot::g_args->engine_type = engine_type;
    if (needCandidates) {
        // TaskSlot::g_args->printCandidates();
    }
    if (needEdgeMatrix) {
        // TaskSlot::g_args->printEdgeMatrix();
    }

    auto split_start = std::chrono::steady_clock::now();
    SplitGraph* multigraphs = 
        new SplitGraph(data_graph, query_graph, JoinParadigm::LEFTDEEP, SplitPattern::SEED, TaskSlot::g_args->splitMode, time_limit);
    auto split_end = std::chrono::steady_clock::now();
    int64_t split_query_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(split_end - split_start).count();


    // multigraphs->printSketchTree();
    // 用SplitGraph里面的InducedSubgraphs来初始化unitArgsVec
    // LOG() << multigraphs;

    LOG() << "subgraph count == " << multigraphs->getSubgraphCount() << std::endl;
    UnitArgs** unitArgsVec = new UnitArgs*[multigraphs->getSubgraphCount()];
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        InducedSubgraph* q_graph = multigraphs->getSubgraphs()[id_unit];
        LOG() << "subgraph " << id_unit << " vertices count == " << q_graph->subgraph->getVerticesCount() << std::endl;
        unitArgsVec[id_unit] = new UnitArgs(q_graph, output_limit);
        if (needCandidates) {
            unitArgsVec[id_unit]->mapUidCandidates(TaskSlot::g_args);
        }
        if (needEdgeMatrix) {
            unitArgsVec[id_unit]->mapUidEdgeMatrix(TaskSlot::g_args);
        }
        // if (engine_type == "CECI") {
        //     unitArgsVec[id_unit]->mapUidCandidatesCECI(TaskSlot::g_args);
        // }
    }

    TaskSlot::g_args->unitArgsVec = unitArgsVec;
    TaskSlot::g_args->count_unit = multigraphs->getSubgraphCount();

    bool needPivot = false;
    bool needBN = true;
    // if (order_type == "EXPLORE" || order_type == "DPiso") {
    //     needPivot = true;
    // }    
    if (engine_type == "EXPLORE" || engine_type == "DPiso") {
        needPivot = true;
    }
    if (engine_type == "CECI") {
        needBN = false;
    }

    if (order_type == "DPiso") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            // u_args->query_graph->printGraph();
            // 每个子图的dpiso_tree都要建一下
            // LOG() << "Generate DPiso tree for unit " << id_unit << std::endl;
            // LOG() << "print unit graph : " << std::endl;
            // q_graph->printGraph();
            /*
                generateDPisoFilterPlan 生成的是 tree_order
                generateDPisoQueryPlan 生成的是 matching_order
             */
            GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, u_args);
            // LOG() << "Generate DPiso order for unit " << id_unit << std::endl;
            GenerateQueryPlan::generateDPisoQueryPlan(q_graph, u_args);
            // GenerateQueryPlan::printWorkloadArray(q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            // std::ostringstream oss;
            // oss.str("");
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit parent: " << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     ui u = u_args->order[i];
            //     oss << u << ',' << u_args->tree[u].parent_ << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // 这里pivot要重新设计一下，有的bn里没有pivot，要从bn里删除这一部分
            // if (needBN) {
            //     if (needPivot) {
            //         LOG() << "get pivot" << std::endl;
            //         GenerateQueryPlan::generatePivot(u_args);
            //         EvaluateQuery::generateBNWithPivot(u_args);
            //     } else {
            //         EvaluateQuery::generateBN(u_args);
            //     }
            // }

            // NOTE: LFTJ这种不用pivot的，没法子做这个检查
            // if (order_type != "Spectrum") {
            //     GenerateQueryPlan::checkQueryPlanCorrectness(q_graph, u_args->order, u_args->pivots);
            //     // GenerateQueryPlan::printSimplifiedQueryPlan(query_graph, matching_order);
            // } else {
            //     LOG() << "Generate " << spectrum.size() << " matching orders." << std::endl;
            // }
        }

        // // original
        // // 这里是因为filter的时候可能是LDF之类的，还没生成这个tree
        // if (dpiso_tree == nullptr) {
        //     GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, query_graph, dpiso_tree, dpiso_order);
        // }
        // GenerateQueryPlan::generateDSPisoQueryPlan(query_graph, edge_matrix, matching_order, pivots, dpiso_tree, dpiso_order,
        //                                             candidates_count, weight_array);
    } else if (order_type == "CIRCINUS") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            GenerateFilteringPlan::generateDPisoFilterPlan(data_graph, u_args);
            // LOG() << "Generate DPiso order for unit " << id_unit << std::endl;
            GenerateQueryPlan::generateDPisoQueryPlan(q_graph, u_args);
            // GenerateQueryPlan::printWorkloadArray(q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            u_args->generateCircinusLayer();
        }
    } else if (order_type == "CECI") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // LOG() << "multigraphs->getSubgraphCount() == " << multigraphs->getSubgraphCount() << std::endl;
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            u_args->tree = TaskSlot::g_args->tree;
            u_args->tree_order = TaskSlot::g_args->tree_order;
            // // copy tree_order
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     u_args->tree_order[i] = TaskSlot::g_args->tree_order[i];
            // }

            // copy tree
            GenerateFilteringPlan::generateCECIFilterPlan(data_graph, u_args);
            GenerateQueryPlan::generateCECIQueryPlan(q_graph, u_args);

            // GenerateQueryPlan::printWorkloadArray(q_graph, u_args);

            // if (needBN) {
            //     if (needPivot) {
            //         GenerateQueryPlan::generatePivot(u_args);
            //         EvaluateQuery::generateBNWithPivot(u_args);
            //     } else {
            //         EvaluateQuery::generateBN(u_args);
            //     }
            // }

            // if (order_type != "Spectrum") {
            //     GenerateQueryPlan::checkQueryPlanCorrectness(q_graph, u_args->order, u_args->pivots);
            //     // GenerateQueryPlan::printSimplifiedQueryPlan(query_graph, matching_order);
            // } else {
            //     LOG() << "Generate " << spectrum.size() << " matching orders." << std::endl;
            // }
            // std::ostringstream oss;
            // oss.str("");
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit parent: " << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     ui u = u_args->order[i];
            //     oss << u << ',' << u_args->tree[u].parent_ << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");
        }
    } else if (order_type == "GQL") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            // TODO: 把这里替换成DPiso_filter之类的，构造出一个tree即可
            GenerateQueryPlan::generateGQLQueryPlan(data_graph, q_graph, u_args);
            // GenerateQueryPlan::printWorkloadArray(q_graph, u_args);
            EvaluateQuery::generateBN(u_args);

            // std::ostringstream oss;
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit bn" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << "bn_count[" << i << "] = " << u_args->bn_count[i] << std::endl;
            //     for (ui j = 0; j < u_args->bn_count[i]; j++) {
            //         oss << u_args->bn[i][j] << ", ";
            //     }
            //     oss << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
        }

    } else if (order_type == "CFL") {
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            // build tree, order, bn, bn_count for unitArgs
            UnitArgs* u_args = unitArgsVec[id_unit];
            Graph* q_graph = u_args->query_graph->subgraph;
            
            GenerateFilteringPlan::generateCFLFilterPlan(data_graph, u_args);
            GenerateQueryPlan::generateCFLQueryPlan(q_graph, u_args);
            // GenerateQueryPlan::printWorkloadArray(q_graph, u_args);
            EvaluateQuery::generateBNWithPivot(u_args);
            // std::ostringstream oss;
            // oss << "log unit order" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << u_args->order[i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            // oss << "log unit bn" << std::endl;
            // for (ui i = 0; i < query_graph->getVerticesCount(); i++) {
            //     oss << "bn_count[" << i << "] = " << u_args->bn_count[i] << std::endl;
            //     for (ui j = 0; j < u_args->bn_count[i]; j++) {
            //         oss << u_args->bn[i][j] << ", ";
            //     }
            //     oss << std::endl;
            // }
            // oss << std::endl;
            // LOG() << oss.str();
        }
    } else {
        LOG() << "The specified order type '" << order_type << "' is not supported." << std::endl;
        abort();
    }

    /*
        print order, candidates, edge_matrix of subgraphs
     */
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        UnitArgs* u_args = unitArgsVec[id_unit];
        for (ui i = 0; i < unitArgsVec[id_unit]->query_graph->subgraph->getVerticesCount(); i++) {
            // LOG() << "order : " << unitArgsVec[id_unit]->order[i] << std::endl;
        }

        // u_args->printCandidates();
        if (engine_type == "CECI") {
            // u_args->printTECandidates();
        } else {
            // u_args->printEdgeMatrix();
        }
    }

    end = std::chrono::steady_clock::now();
    int64_t generate_query_plan_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    LOG() << "-----" << std::endl;
    LOG() << "Enumerate..." << std::endl;

    // exit(0);
    // continue;


#if ENABLE_QFLITER == 1
    EvaluateQuery::qfliter_bsr_graph_ = BuildTable::qfliter_bsr_graph_;
#endif

    /**
     * Engine Start here
     */
    // TODO: change all pointer to smart pointer
    // shallow copy the global info

    start = std::chrono::steady_clock::now();

    /**
     * NOTE: 先 16x 这么设置，再去原文里看看有没有能拿过来用的 setting
     */
    int64_t queue_capacity = num_threads * 16;
    // if (input_type_schedule == "static") {
    //     ui start_vertex = unitArgsVec[0]->order[0];
    //     queue_capacity = unitArgsVec[0]->candidates_count[start_vertex];
    //     // LOG() << "start_vertex == " << start_vertex << std::endl;
    //     // LOG() << "queue_capacity == " << queue_capacity << std::endl;
    // } else {
    //     queue_capacity = num_threads * 16;
    // }

    TaskPool* taskPool = new TaskPool(num_threads, queue_capacity);
    /**
     * static 的 capacity设置为无穷大
    */ 
    if (input_type_schedule == "static" || input_type_schedule == "staticworkload") {
        taskPool->QUEUE_CAPACITY = std::numeric_limits<int64_t>::max();
    }

    // LOG() << "queue_capacity == " << taskPool->QUEUE_CAPACITY << std::endl;

    if (input_type_schedule == "static") {
        taskPool->schedule_type = ScheduleType::STATIC;
    } else if (input_type_schedule == "staticworkload") {
        taskPool->schedule_type = ScheduleType::STATICWORKLOAD;
    } else if (input_type_schedule == "busy2idlenostop") {
        taskPool->schedule_type = ScheduleType::BUSY2IDLENOSTOP;
    } else if (input_type_schedule == "busy2idledepthstop") {
        taskPool->schedule_type = ScheduleType::BUSY2IDLEDEPTHSTOP;
    } else if (input_type_schedule == "timeout") {
        taskPool->schedule_type = ScheduleType::TIMEOUT;
    } else {
        LOG() << "Error: The schedule type is not defined." << std::endl;
        abort();
    }

    if (input_type_split == "linear") {
        taskPool->split_type = SplitType::LINEAR;
    } else if (input_type_split == "upperone") {
        taskPool->split_type = SplitType::UPPERONE;
    } else if (input_type_split == "workload") {
        taskPool->split_type = SplitType::WORKLOAD;
    } else if (input_type_split == "layer") {
        taskPool->split_type = SplitType::LAYER;
    } else {
        LOG() << "Error: The split type is not defined." << std::endl;
        abort();
    }

    LOG() << "schedule_type == " << input_type_schedule << std::endl;
    LOG() << "split_type == " << input_type_split << std::endl;

    std::string input_type_split = command.getSplitType();
    std::string input_type_schedule = command.getScheduleType();

    // TODO: split_type and schedule_type
    if (engine_type == "LFTJ") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::LFTJ(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "GQL") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreGraphQLStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "EXPLORE") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreGraph(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "DPiso") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreDPisoStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else if (engine_type == "CECI") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreCECIStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
        for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
            unitArgsVec[id_unit]->tree = nullptr;
            unitArgsVec[id_unit]->tree_order = nullptr;
        }
    } else if (engine_type == "CIRCINUS") {
        EvaluateQuery::enum_method = [taskPool](TaskSlot& task, int task_id) { EvaluateQuery::exploreCircinusStyle(taskPool, task, task_id); };
        EvaluateQuery::ParallelExecute(taskPool, multigraphs, unitArgsVec);
    } else {
        LOG() << "Not supported engine type: " << engine_type << std::endl;
    }
    end = std::chrono::steady_clock::now();
    int64_t enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // LOG() << "over backtracking" << std::endl;

    // LOG() << "partial_matches sizes : " << std::endl;
    // for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
    //     LOG() << "unit " << id_unit << ": " << unitArgsVec[id_unit]->unit_partial_match->data.size() / unitArgsVec[id_unit]->unit_partial_match->length << std::endl;
    // }

    // multigraphs->printPaths();

    // JoinCollect* jc = new JoinCollect(data_graph, query_graph, multigraphs->jtree, multigraphs->getSubgraphCount(), TaskSlot::g_args->time_limit, taskPool);
    JoinCollect* jc = new JoinCollect(data_graph, query_graph, multigraphs->jseq, multigraphs->getSubgraphCount(), TaskSlot::g_args->time_limit, taskPool);
    // LOG
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        jc->partial_matches[id_unit] = unitArgsVec[id_unit]->unit_partial_match;
        // LOG() << "unit " << id_unit << " partial match size: "
        //       << unitArgsVec[id_unit]->unit_partial_match->data.size() / int64_t(unitArgsVec[id_unit]->unit_partial_match->length)
        //       << std::endl;
    }

    // LOG() << "jointree_root == " << multigraphs->jtree->jointree_root << std::endl;
    // PartialMatch* res = jc->recur_collect(multigraphs->jtree->jointree_root);
    PartialMatch* res = jc->sequence_collect();
    if (res && SPLITMODE::Q == TaskSlot::g_args->splitMode) {
        // size_t embedding_count = res->size;
        // LOG() << "joined embedding count : " << embedding_count << std::endl;
        TaskSlot::g_args->embedding_count = res->size;
    } else {
        // LOG() << "nullptr joined embedding count : 0, not finished" << std::endl;
    }
    if (jc->overtime) {
        // LOG() << "Query Time out." << std::endl;
    }
    delete jc;
    delete res;

#ifdef DISTRIBUTION
    std::ofstream outfile (input_distribution_file_path , std::ofstream::binary);
    outfile.write((char*)EvaluateQuery::distribution_count_, sizeof(size_t) * data_graph->getVerticesCount());
    delete[] EvaluateQuery::distribution_count_;
#endif

    /**
     * End.
     */
    // embedding_count = TaskSlot::g_args->embedding_count;
    // call_count = TaskSlot::g_args->call_count;
    // LOG() << "--------------------------------------------------------------------" << std::endl;
    int64_t preprocessing_time_in_ns = filter_vertices_time_in_ns + build_table_time_in_ns + generate_query_plan_time_in_ns;
    int64_t total_time_in_ns = preprocessing_time_in_ns + enumeration_time_in_ns;

    TaskSlot::g_args->load_graphs_time_in_ns = load_graphs_time_in_ns;
    TaskSlot::g_args->filter_vertices_time_in_ns = filter_vertices_time_in_ns;
    TaskSlot::g_args->split_query_time_in_ns = split_query_time_in_ns;
    TaskSlot::g_args->build_table_time_in_ns = build_table_time_in_ns;
    TaskSlot::g_args->generate_query_plan_time_in_ns = generate_query_plan_time_in_ns;
    TaskSlot::g_args->enumeration_time_in_ns = enumeration_time_in_ns;
    TaskSlot::g_args->preprocessing_time_in_ns = preprocessing_time_in_ns;
    TaskSlot::g_args->total_time_in_ns = total_time_in_ns;
    TaskSlot::g_args->memory_cost_in_bytes = memory_cost_in_bytes;
    TaskSlot::g_args->total_task_num = taskPool->total_task_num;
    
    TaskSlot::g_args->logGlobalStatistics();

    // delete query_graph;
    // delete data_graph;
    // destroy below resources in GLOBALARGS destructor



    // LOG() << "--------------------------------------------------------------------" << std::endl;
    // LOG() << "Release memories..." << std::endl;
    /**
     * Release the allocated memories.
     */
    // delete[] candidates_count;
    delete[] tso_order;
    delete[] tso_tree;
    delete[] cfl_order;
    delete[] cfl_tree;
    delete[] dpiso_order;
    delete[] dpiso_tree;
    delete[] ceci_order;
    delete[] ceci_tree;
    // delete[] matching_order;
    delete[] pivots;
    if (weight_array != nullptr) {
        for (ui i = 0; i < query_graph->getVerticesCount(); ++i) {
            delete[] weight_array[i];
        }
        delete[] weight_array;
    }

    delete taskPool;
    for (int id_unit = 0; id_unit < multigraphs->getSubgraphCount(); id_unit++) {
        delete unitArgsVec[id_unit];
    }
    delete[] unitArgsVec;
    delete TaskSlot::g_args;
    delete multigraphs;
    edge_matrix = nullptr;
    candidates = nullptr;
    candidates_count = nullptr;
    matching_order = nullptr;
} // end of method loop
} // end of thread loop

    delete query_graph;
    query_graph = nullptr;

    delete data_graph;
    data_graph = nullptr;
}


int main(int argc, char** argv) {
    MatchingCommand command(argc, argv);
    std::string input_QorCandi_split = command.getQorCandiType();
    // Q 就默认是Q&C了
    if (input_QorCandi_split == "Q") {
        // TaskSlot::g_args->splitMode = SPLITMODE::Q;
        split_Q_test(command);
    } else if (input_QorCandi_split == "C"){
        // TaskSlot::g_args->splitMode = SPLITMODE::C;
        split_C_test(command);
    } else {
        LOG() << "Error: The split mode is not defined." << std::endl;
        abort();
    }

    /*
    解析这三个东西
        num_threads=("1" "2" "4" "8" "16" "32" )
        methods_split=("shixuan" )
        methods_schedule=("steal" )
     */

    /**
     * Output the command line information.
     */

    // std::vector<std::string> filter_set{"DPiso"};
    // std::vector<std::string> order_set{"CIRCINUS"};
    // std::vector<std::string> engine_set{"CIRCINUS"};

    // std::vector<std::string> filter_set{"DPiso"};
    // std::vector<std::string> order_set{"DPiso"};
    // std::vector<std::string> engine_set{"LFTJ"};


    /**
     * Load input graphs.
     */
    // LOG() << "Load graphs..." << std::endl;

    return 0;
}