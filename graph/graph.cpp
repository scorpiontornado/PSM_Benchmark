#include "graph.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <utility/graphoperations.h>


/**
    reverse_index_ 存储所有顶点 ID，按照标签顺序分组。
    reverse_index_offsets_ 表示每个标签的起始和终止范围。
    reverse_index_ = [0, 4, 1, 2, 5, 3]（按标签 0 -> 1 -> 2 的顺序存储顶点）
    reverse_index_offsets_ = [0, 2, 5, 6]（标签 0 的范围为 [0, 2)，标签 1 的范围为 [2, 5)，标签 2 的范围为 [5, 6)）。
 */
void Graph::BuildReverseIndex() {
    reverse_index_ = new ui[vertices_count_];
    reverse_index_offsets_= new ui[labels_count_ + 1];
    reverse_index_offsets_[0] = 0;

    ui total = 0;
    for (ui i = 0; i < labels_count_; ++i) {
        reverse_index_offsets_[i + 1] = total;
        // std::cout << "label : " << i << ", offset : " << reverse_index_offsets_[i + 1] << std::endl;
        total += labels_frequency_[i];
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID label = labels_[i];
        // std::cout << "u_id : " << i << ", label : " << label << std::endl;
        // std::cout << reverse_index_offsets_[label + 1] << std::endl;
        reverse_index_[reverse_index_offsets_[label + 1]++] = i;
    }
}

#if OPTIMIZED_LABELED_GRAPH == 1
void Graph::BuildNLF() {
    nlf_ = new std::unordered_map<LabelID, ui>[vertices_count_];
    for (ui i = 0; i < vertices_count_; ++i) {
        ui count;
        const VertexID * neighbors = getVertexNeighbors(i, count);

        for (ui j = 0; j < count; ++j) {
            VertexID u = neighbors[j];
            LabelID label = getVertexLabel(u);
            if (nlf_[i].find(label) == nlf_[i].end()) {
                nlf_[i][label] = 0;
            }

            nlf_[i][label] += 1;
        }
    }
}

void Graph::BuildLabelOffset() {
    size_t labels_offset_size = (size_t)vertices_count_ * labels_count_ + 1;
    labels_offsets_ = new ui[labels_offset_size];
    std::fill(labels_offsets_, labels_offsets_ + labels_offset_size, 0);

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1],
            [this](const VertexID u, const VertexID v) -> bool {
                return labels_[u] == labels_[v] ? u < v : labels_[u] < labels_[v];
            });
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID previous_label = 0;
        LabelID current_label = 0;

        labels_offset_size = i * labels_count_;
        labels_offsets_[labels_offset_size] = offsets_[i];

        for (ui j = offsets_[i]; j < offsets_[i + 1]; ++j) {
            current_label = labels_[neighbors_[j]];

            if (current_label != previous_label) {
                for (ui k = previous_label + 1; k <= current_label; ++k) {
                    labels_offsets_[labels_offset_size + k] = j;
                }
                previous_label = current_label;
            }
        }

        for (ui l = current_label + 1; l <= labels_count_; ++l) {
            labels_offsets_[labels_offset_size + l] = offsets_[i + 1];
        }
    }
}

#endif

void Graph::loadGraphFromFile(const std::string &file_path) {
    std::ifstream infile(file_path);

    if (!infile.is_open()) {
        std::cout << "Can not open the graph file " << file_path << " ." << std::endl;
        exit(-1);
    }

    char type;
    infile >> type >> vertices_count_ >> edges_count_;
    offsets_ = new ui[vertices_count_ +  1];
    offsets_[0] = 0;

    neighbors_ = new VertexID[edges_count_ * 2];
    labels_ = new LabelID[vertices_count_];
    labels_count_ = 0;
    max_degree_ = 0;

    LabelID max_label_id = 0;
    std::vector<ui> neighbors_offset(vertices_count_, 0);

    while (infile >> type) {
        if (type == 'v') { // Read vertex.
            VertexID id;
            LabelID  label;
            ui degree;
            infile >> id >> label >> degree;

            labels_[id] = label;
            offsets_[id + 1] = offsets_[id] + degree;

            if (degree > max_degree_) {
                max_degree_ = degree;
            }

            if (labels_frequency_.find(label) == labels_frequency_.end()) {
                labels_frequency_[label] = 0;
                if (label > max_label_id)
                    max_label_id = label;
            }

            labels_frequency_[label] += 1;
        }
        else if (type == 'e') { // Read edge.
            VertexID begin;
            VertexID end;
            infile >> begin >> end;

            ui offset = offsets_[begin] + neighbors_offset[begin];
            neighbors_[offset] = end;

            offset = offsets_[end] + neighbors_offset[end];
            neighbors_[offset] = begin;

            neighbors_offset[begin] += 1;
            neighbors_offset[end] += 1;
        }
    }

    infile.close();
    labels_count_ = (ui)labels_frequency_.size() > (max_label_id + 1) ? (ui)labels_frequency_.size() : max_label_id + 1;

    for (auto element : labels_frequency_) {
        if (element.second > max_label_frequency_) {
            max_label_frequency_ = element.second;
        }
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1]);
    }

    BuildReverseIndex();

#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
}

void Graph::printGraphMetaData() {
    std::cout << "|V|: " << vertices_count_ << ", |E|: " << edges_count_ << ", |\u03A3|: " << labels_count_ << std::endl;
    std::cout << "Max Degree: " << max_degree_ << ", Max Label Frequency: " << max_label_frequency_ << std::endl;
}

void Graph::printGraph() {
    std::ostringstream oss;

    // 输出顶点信息
    oss << "Vertices: " << std::endl;
    for (ui i = 0; i < vertices_count_; ++i) {
        oss << i << ": " 
                      << "labels_=" << labels_[i] 
                      << ", degree=" << getVertexDegree(i) 
                      << std::endl;
    }

    // 输出边信息
    oss << "Edges: " << std::endl;
    for (ui i = 0; i < vertices_count_; ++i) {
        oss << "u " << i << ": ";
        ui count;
        const VertexID* neighbors = getVertexNeighbors(i, count);
        for (ui j = 0; j < count; ++j) {
            oss << neighbors[j];
            if (j < count - 1) {
                oss << ", ";
            }
        }
        oss << std::endl;
    }

    // 一次性输出
    LOG() << oss.str();
}

void Graph::buildCoreTable() {
    core_table_ = new int[vertices_count_];
    GraphOperations::getKCore(this, core_table_);

    for (ui i = 0; i < vertices_count_; ++i) {
        if (core_table_[i] > 1) {
            core_length_ += 1;
        }
    }
}

void Graph::loadGraphFromFileCompressed(const std::string &degree_path, const std::string &edge_path,
                                        const std::string &label_path) {
    std::ifstream deg_file(degree_path, std::ios::binary);

    if (deg_file.is_open()) {
        std::cout << "Open degree file " << degree_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open degree file " << degree_path << " ." << std::endl;
        exit(-1);
    }

    auto start = std::chrono::high_resolution_clock::now();
    int int_size;
    deg_file.read(reinterpret_cast<char *>(&int_size), 4);
    deg_file.read(reinterpret_cast<char *>(&vertices_count_), 4);
    deg_file.read(reinterpret_cast<char *>(&edges_count_), 4);

    offsets_ = new ui[vertices_count_ + 1];
    ui* degrees = new unsigned int[vertices_count_];

    deg_file.read(reinterpret_cast<char *>(degrees), sizeof(int) * vertices_count_);


    deg_file.close();
    deg_file.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Load degree file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;

    std::ifstream adj_file(edge_path, std::ios::binary);

    if (adj_file.is_open()) {
        std::cout << "Open edge file " << edge_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open edge file " << edge_path << " ." << std::endl;
        exit(-1);
    }

    start = std::chrono::high_resolution_clock::now();
    size_t neighbors_count = (size_t)edges_count_ * 2;
    neighbors_ = new ui[neighbors_count];

    offsets_[0] = 0;
    for (ui i = 1; i <= vertices_count_; ++i) {
        offsets_[i] = offsets_[i - 1] + degrees[i - 1];
    }

    max_degree_ = 0;

    for (ui i = 0; i < vertices_count_; ++i) {
        if (degrees[i] > 0) {
            if (degrees[i] > max_degree_)
                max_degree_ = degrees[i];
            adj_file.read(reinterpret_cast<char *>(neighbors_ + offsets_[i]), degrees[i] * sizeof(int));
            std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1]);
        }
    }

    adj_file.close();
    adj_file.clear();

    delete[] degrees;

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Load adj file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;


    std::ifstream label_file(label_path, std::ios::binary);
    if (label_file.is_open())  {
        std::cout << "Open label file " << label_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open label file " << label_path << " ." << std::endl;
        exit(-1);
    }

    start = std::chrono::high_resolution_clock::now();

    labels_ = new ui[vertices_count_];
    label_file.read(reinterpret_cast<char *>(labels_), sizeof(int) * vertices_count_);

    label_file.close();
    label_file.clear();

    ui max_label_id = 0;
    for (ui i = 0; i < vertices_count_; ++i) {
        ui label = labels_[i];

        if (labels_frequency_.find(label) == labels_frequency_.end()) {
            labels_frequency_[label] = 0;
            if (label > max_label_id)
                max_label_id = label;
        }

        labels_frequency_[label] += 1;
    }

    labels_count_ = (ui)labels_frequency_.size() > (max_label_id + 1) ? (ui)labels_frequency_.size() : max_label_id + 1;

    for (auto element : labels_frequency_) {
        if (element.second > max_label_frequency_) {
            max_label_frequency_ = element.second;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Load label file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    BuildReverseIndex();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Build reverse index file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;
#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
}

void Graph::storeComparessedGraph(const std::string& degree_path, const std::string& edge_path,
                                  const std::string& label_path) {
    ui* degrees = new ui[vertices_count_];
    for (ui i = 0; i < vertices_count_; ++i) {
        degrees[i] = offsets_[i + 1] - offsets_[i];
    }

    std::ofstream deg_outputfile(degree_path, std::ios::binary);

    if (deg_outputfile.is_open()) {
        std::cout << "Open degree file " << degree_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot degree edge file " << degree_path << " ." << std::endl;
        exit(-1);
    }

    int int_size = sizeof(int);
    size_t vertex_array_bytes = ((size_t)vertices_count_) * 4;
    deg_outputfile.write(reinterpret_cast<const char *>(&int_size), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(&vertices_count_), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(&edges_count_), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(degrees), vertex_array_bytes);

    deg_outputfile.close();
    deg_outputfile.clear();

    delete[] degrees;

    std::ofstream edge_outputfile(edge_path, std::ios::binary);

    if (edge_outputfile.is_open()) {
        std::cout << "Open edge file " << edge_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot edge file " << edge_path << " ." << std::endl;
        exit(-1);
    }

    size_t edge_array_bytes = ((size_t)edges_count_ * 2) * 4;
    edge_outputfile.write(reinterpret_cast<const char *>(neighbors_), edge_array_bytes);

    edge_outputfile.close();
    edge_outputfile.clear();

    std::ofstream label_outputfile(label_path, std::ios::binary);

    if (label_outputfile.is_open()) {
        std::cout << "Open label file " << label_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot label file " << label_path << " ." << std::endl;
        exit(-1);
    }

    size_t label_array_bytes = ((size_t)vertices_count_) * 4;
    label_outputfile.write(reinterpret_cast<const char *>(labels_), label_array_bytes);

    label_outputfile.close();
    label_outputfile.clear();
}

void Graph::loadsubgFromGraph(Graph* graph, std::vector<ui>& vertices, std::vector<std::pair<ui, ui>>& edges, ui* old2new, ui* new2old, ui* l_old2new, ui* l_new2old) {
    // 清理当前图的数据
    // LOG() << "Clearing current graph data..." << std::endl;
    if (offsets_) delete[] offsets_;
    if (neighbors_) delete[] neighbors_;
    if (labels_) delete[] labels_;
    if (reverse_index_offsets_) delete[] reverse_index_offsets_;
    if (reverse_index_) delete[] reverse_index_;
    if (core_table_) delete[] core_table_;

    offsets_ = nullptr;
    neighbors_ = nullptr;
    labels_ = nullptr;
    reverse_index_offsets_ = nullptr;
    reverse_index_ = nullptr;
    core_table_ = nullptr;
    labels_frequency_.clear();

    vertices_count_ = 0;
    edges_count_ = 0;
    labels_count_ = 0;
    max_degree_ = 0;
    max_label_frequency_ = 0;
    core_length_ = 0;

    // 初始化 old2new 和 new2old 数组
    // LOG() << "Initializing old2new and new2old arrays..." << std::endl;
    ui big_graph_vertex_count = graph->getVerticesCount();
    ui subgraph_vertex_count = vertices.size();

    // 假设 old2new 和 new2old 已由调用者分配好
    std::fill(old2new, old2new + big_graph_vertex_count, INVALID_VERTEX_ID);
    std::fill(new2old, new2old + subgraph_vertex_count, INVALID_VERTEX_ID);

    // 创建父图顶点ID到子图顶点ID的映射，同时更新 old2new 和 new2old 数组
    // LOG() << "Mapping old vertex IDs to new vertex IDs..." << std::endl;
    std::unordered_map<ui, ui> old_to_new;
    ui new_vertex_id = 0;
    for (ui old_vertex_id : vertices) {
        old_to_new[old_vertex_id] = new_vertex_id;
        old2new[old_vertex_id] = new_vertex_id;  // 更新 old2new 映射
        new2old[new_vertex_id] = old_vertex_id;  // 更新 new2old 映射
        new_vertex_id++;
    }

    // std::ostringstream oss;
    // oss << "print idMaps" << std::endl;
    // for (ui old_vertex_id : vertices) {
    //     oss << old_vertex_id << ", ";
    // }
    // oss << std::endl;
    // for (ui old_vertex_id : vertices) {
    //     oss << old2new[old_vertex_id] << ", ";
    // }
    // oss << std::endl;
    // for (ui id = 0; id < new_vertex_id; id++) {
    //     oss << new2old[id] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // oss.str("");
    // oss.clear();
    // oss << "print edges : " << std::endl;
    // for (auto& e: edges) {
    //     oss << e.first << " -> " << e.second << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();


    vertices_count_ = subgraph_vertex_count;
    offsets_ = new ui[vertices_count_ + 1];
    labels_ = new LabelID[vertices_count_];
    offsets_[0] = 0;

    std::vector<VertexID> neighbors_list; // 用于暂存邻居信息

    // 遍历子图中的每个顶点，构建邻接列表
    std::unordered_map<ui, ui> degree_map;

    std::unordered_set<LabelID> labels_set;
    // LOG() << "Processing vertices and copying labels..." << std::endl;
    for (ui i = 0; i < vertices_count_; ++i) {
        ui old_vertex_id = vertices[i];

        // 复制标签
        LabelID label = graph->getVertexLabel(old_vertex_id);
        if (labels_set.find(label) == labels_set.end()) {
            // LOG() << "i == " << i << ", old_vertex_id == " << old_vertex_id << ", label == " << label << std::endl;
            // LOG() << "label_set.size == " << labels_set.size() << std::endl;
            // LOG() << "label == " << label << std::endl;
            labels_set.insert(label);
            l_old2new[label] = labels_set.size() - 1;
            l_new2old[labels_set.size() - 1] = label;
        }
        label = l_old2new[label];
        labels_[i] = label;

        // 更新标签频率
        labels_frequency_[label] += 1;
    }

    // LOG() << "Processing edges..." << std::endl;
    // 处理边集，构建邻接列表
    for (const auto& edge : edges) {
        ui src_old = edge.first;
        ui dst_old = edge.second;

        // 确保边的两个端点都在子图中
        if (old_to_new.find(src_old) != old_to_new.end() && old_to_new.find(dst_old) != old_to_new.end()) {
            ui src_new = old_to_new[src_old];
            ui dst_new = old_to_new[dst_old];

            neighbors_list.push_back(dst_new);
            
            degree_map[src_new]++;
        } else {
            LOG() << "Error: Edge (" << src_old << ", " << dst_old << ") contains vertex not in the subgraph." << std::endl;
        }
    }

    // LOG() << "Filling offsets based on degree map..." << std::endl;
    // 根据 degree_map 填充 offsets_
    for (ui i = 0; i < vertices_count_; ++i) {
        offsets_[i + 1] = offsets_[i] + degree_map[i];

        // 更新最大度数
        if (degree_map[i] > max_degree_) {
            max_degree_ = degree_map[i];
        }
    }

    // 更新边数量
    edges_count_ = edges.size();

    // 更新标签数量
    labels_count_ = labels_frequency_.size();

    // 更新最大标签频率
    max_label_frequency_ = 0;
    for (const auto& pair : labels_frequency_) {
        if (pair.second > max_label_frequency_) {
            max_label_frequency_ = pair.second;
        }
    }

    // LOG() << "Allocating neighbors array and copying data..." << std::endl;
    // 分配 neighbors_ 数组并复制邻居数据
    neighbors_ = new ui[neighbors_list.size()];
    std::copy(neighbors_list.begin(), neighbors_list.end(), neighbors_);

    // 对每个顶点的邻居进行排序
    // LOG() << "Sorting neighbors for each vertex..." << std::endl;
    for (ui i = 0; i < vertices_count_; ++i) {
        ui start = offsets_[i];
        ui end = offsets_[i + 1];
        std::sort(neighbors_ + start, neighbors_ + end);
    }

    // 构建反向索引
    // LOG() << "Building reverse index..." << std::endl;
    BuildReverseIndex();

    // 构建 core_table
    // LOG() << "Building core table..." << std::endl;
    buildCoreTable();

    // 如果需要，构建其他辅助结构
#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        // LOG() << "Building NLF and label offsets..." << std::endl;
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
    // LOG() << "Subgraph loading completed." << std::endl;
}


void Graph::loadsubgFromGraph(Graph* graph, std::vector<ui>& path, ui* old2new, ui* new2old, ui* l_old2new, ui* l_new2old) {
    // 清理当前图的数据
    if (offsets_) delete[] offsets_;
    if (neighbors_) delete[] neighbors_;
    if (labels_) delete[] labels_;
    if (reverse_index_offsets_) delete[] reverse_index_offsets_;
    if (reverse_index_) delete[] reverse_index_;
    if (core_table_) delete[] core_table_;

    offsets_ = nullptr;
    neighbors_ = nullptr;
    labels_ = nullptr;
    reverse_index_offsets_ = nullptr;
    reverse_index_ = nullptr;
    core_table_ = nullptr;
    labels_frequency_.clear();

    vertices_count_ = 0;
    edges_count_ = 0;
    labels_count_ = 0;
    max_degree_ = 0;
    max_label_frequency_ = 0;
    core_length_ = 0;

    // 初始化 old2new 和 new2old 数组
    ui big_graph_vertex_count = graph->getVerticesCount();
    ui subgraph_vertex_count = path.size();

    // 假设 old2new 和 new2old 已由调用者分配好
    // 将 old2new 初始化为 INVALID_VERTEX_ID
    std::fill(old2new, old2new + big_graph_vertex_count, INVALID_VERTEX_ID);
    std::fill(new2old, new2old + subgraph_vertex_count, INVALID_VERTEX_ID);

    std::unordered_set<LabelID> labels_set;

    // 创建父图顶点ID到子图顶点ID的映射，同时更新 old2new 和 new2old 数组
    std::unordered_map<ui, ui> old_to_new;
    ui new_vertex_id = 0;
    for (ui old_vertex_id : path) {
        old_to_new[old_vertex_id] = new_vertex_id;
        old2new[old_vertex_id] = new_vertex_id;  // 更新 old2new 映射
        new2old[new_vertex_id] = old_vertex_id;  // 更新 new2old 映射
        new_vertex_id++;
    }

    // std::ostringstream oss;
    // oss << "print idMaps" << std::endl;
    // for (ui old_vertex_id : path) {
    //     oss << old_vertex_id << ", ";
    // }
    // oss << std::endl;
    // for (ui old_vertex_id : path) {
    //     oss << old2new[old_vertex_id] << ", ";
    // }
    // oss << std::endl;
    // for (ui id = 0; id < new_vertex_id; id++) {
    //     oss << new2old[id] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    vertices_count_ = path.size();
    offsets_ = new ui[vertices_count_ + 1];
    labels_ = new LabelID[vertices_count_];
    offsets_[0] = 0;

    std::vector<VertexID> neighbors_list; // 用于暂存邻居信息

    // 遍历子图中的每个顶点，构建邻接列表
    for (ui i = 0; i < vertices_count_; ++i) {
        ui old_vertex_id = path[i];
        ui degree = 0;

        // 复制标签
        LabelID label = graph->getVertexLabel(old_vertex_id);
        if (labels_set.find(label) == labels_set.end()) {
            labels_set.insert(label);
            l_old2new[label] = labels_set.size() - 1;
            l_new2old[labels_set.size() - 1] = label;
        }
        label = l_old2new[label];
        labels_[i] = label;

        // 更新标签频率
        labels_frequency_[label] += 1;

        // 获取父图中当前顶点的邻居
        ui neighbor_count = 0;
        const ui* neighbors = graph->getVertexNeighbors(old_vertex_id, neighbor_count);

        // 遍历邻居，检查是否在子图中
        for (ui j = 0; j < neighbor_count; ++j) {
            ui neighbor_old_id = neighbors[j];
            if (old_to_new.find(neighbor_old_id) != old_to_new.end()) {
                // 邻居在子图中，添加到邻居列表
                ui neighbor_new_id = old_to_new[neighbor_old_id];
                neighbors_list.push_back(neighbor_new_id);
                degree++;
            }
        }

        // 更新 offsets_
        offsets_[i + 1] = offsets_[i] + degree;

        // 更新最大度数
        if (degree > max_degree_) {
            max_degree_ = degree;
        }
    }

    // 更新边数量（对于无向图，边数为邻居数的一半）
    edges_count_ = neighbors_list.size() / 2;

    // 更新标签数量
    labels_count_ = labels_frequency_.size();

    // 更新最大标签频率
    max_label_frequency_ = 0;
    // for (const auto& pair : labels_frequency_) {
    //     std::cout << "Label " << pair.first << " frequency: " << pair.second << std::endl;
    //     if (pair.second > max_label_frequency_) {
    //         max_label_frequency_ = pair.second;
    //     }
    // }

    // 分配 neighbors_ 数组并复制邻居数据
    neighbors_ = new ui[neighbors_list.size()];
    std::copy(neighbors_list.begin(), neighbors_list.end(), neighbors_);

    // 对每个顶点的邻居进行排序
    for (ui i = 0; i < vertices_count_; ++i) {
        ui start = offsets_[i];
        ui end = offsets_[i + 1];
        std::sort(neighbors_ + start, neighbors_ + end);
    }

    // 构建反向索引
    BuildReverseIndex();

    // 构建 core_table
    buildCoreTable();

    // 如果需要，构建其他辅助结构
#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
}
