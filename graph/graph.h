#ifndef SUBGRAPHMATCHING_GRAPH_H
#define SUBGRAPHMATCHING_GRAPH_H

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <iostream>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstdlib>

#include "configuration/types.h"
#include "configuration/config.h"
#include "utility/statistics/Logs.h"

/**
 * A graph is stored as the CSR format.
 */

class Graph {
private:
    bool enable_label_offset_;

    ui vertices_count_;
    ui edges_count_;
    ui labels_count_;
    ui max_degree_;
    ui max_label_frequency_;

    ui* offsets_;
    VertexID * neighbors_;
    LabelID* labels_;
    // 下面两个是为了快速获取某个label的所有点
    ui* reverse_index_offsets_;
    ui* reverse_index_;

    // 存储每个点的core数
    int* core_table_;
    // core数大于1的点的个数，也就是图的核心部分的点的数量
    ui core_length_;

    std::unordered_map<LabelID, ui> labels_frequency_;

#if OPTIMIZED_LABELED_GRAPH == 1
    // 这个没用上
    ui* labels_offsets_;
    // nlf[i][labelID] = cnt，存储的是点i的labelID邻居的数量
    std::unordered_map<LabelID, ui>* nlf_;
#endif

private:
    void BuildReverseIndex();

#if OPTIMIZED_LABELED_GRAPH == 1
    void BuildNLF();
    void BuildLabelOffset();
#endif

public:
    Graph(const bool enable_label_offset) {
        enable_label_offset_ = enable_label_offset;

        vertices_count_ = 0;
        edges_count_ = 0;
        labels_count_ = 0;
        max_degree_ = 0;
        max_label_frequency_ = 0;
        core_length_ = 0;

        offsets_ = nullptr;
        neighbors_ = nullptr;
        labels_ = nullptr;
        reverse_index_offsets_ = nullptr;
        reverse_index_ = nullptr;
        core_table_ = nullptr;
        labels_frequency_.clear();

#if OPTIMIZED_LABELED_GRAPH == 1
        labels_offsets_ = nullptr;
        nlf_ = nullptr;
#endif
    }

    ~Graph() {
        delete[] offsets_;
        delete[] neighbors_;
        delete[] labels_;
        delete[] reverse_index_offsets_;
        delete[] reverse_index_;
        delete[] core_table_;
#if OPTIMIZED_LABELED_GRAPH == 1
        delete[] labels_offsets_;
        delete[] nlf_;
#endif
    }

public:
    void loadGraphFromFile(const std::string& file_path);
    void loadGraphFromFileCompressed(const std::string& degree_path, const std::string& edge_path,
                                     const std::string& label_path);
    void storeComparessedGraph(const std::string& degree_path, const std::string& edge_path,
                               const std::string& label_path);
    void printGraphMetaData();
    void printGraph();
public:
    const ui getLabelsCount() const {
        return labels_count_;
    }

    const ui getVerticesCount() const {
        return vertices_count_;
    }

    const ui getEdgesCount() const {
        return edges_count_;
    }

    const ui getGraphMaxDegree() const {
        return max_degree_;
    }

    const ui getGraphMaxLabelFrequency() const {
        return max_label_frequency_;
    }

    const ui getVertexDegree(const VertexID id) const {
        return offsets_[id + 1] - offsets_[id];
    }

    const ui getMaxDegreeVid() const {
        ui max_d = 0, max_id = 0;
        ui v_cnt = getVerticesCount();

        for (ui i = 1; i < v_cnt; i++) {
            if (getVertexDegree(i) > max_d) {
                max_d = getVertexDegree(i);
                max_id = i;
            }
        }
        return max_id;
    }

    void loadsubgFromGraph(Graph* graph, std::vector<ui>& vertices, std::vector<std::pair<ui, ui>>& edges, ui* old2new, ui* new2old, ui* l_old2new, ui* l_new2old);
    void loadsubgFromGraph(Graph* graph, std::vector<ui>& path, ui* old2new, ui* new2old, ui* l_old2new, ui* l_new2old);

    const ui getLabelsFrequency(const LabelID label) const {
        return labels_frequency_.find(label) == labels_frequency_.end() ? 0 : labels_frequency_.at(label);
    }

    const ui getCoreValue(const VertexID id) const {
        return core_table_[id];
    }

    const ui get2CoreSize() const {
        return core_length_;
    }
    const LabelID getVertexLabel(const VertexID id) const {
        return labels_[id];
    }

    const ui * getVertexNeighbors(const VertexID id, ui& count) const {
        count = offsets_[id + 1] - offsets_[id];
        return neighbors_ + offsets_[id];
    }

    const ui * getVerticesByLabel(const LabelID id, ui& count) const {
        count = reverse_index_offsets_[id + 1] - reverse_index_offsets_[id];
        return reverse_index_ + reverse_index_offsets_[id];
    }

#if OPTIMIZED_LABELED_GRAPH == 1
    const ui * getNeighborsByLabel(const VertexID id, const LabelID label, ui& count) const {
        ui offset = id * labels_count_ + label;
        count = labels_offsets_[offset + 1] - labels_offsets_[offset];
        return neighbors_ + labels_offsets_[offset];
    }

    const std::unordered_map<LabelID, ui>* getVertexNLF(const VertexID id) const {
        return nlf_ + id;
    }

    bool checkEdgeExistence(const VertexID u, const VertexID v, const LabelID u_label) const {
        ui count = 0;
        const VertexID* neighbors = getNeighborsByLabel(v, u_label, count);
        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }
#endif

    bool checkEdgeExistence(VertexID u, VertexID v) const {
        if (getVertexDegree(u) < getVertexDegree(v)) {
            std::swap(u, v);
        }
        // LOG() << "u == " << u << ", v == " << v << std::endl;
        ui count = 0;
        const VertexID* neighbors =  getVertexNeighbors(v, count);
        // LOG() << "count == " << count << std::endl;
        // std::ostringstream oss;
        // oss << "neighbors : " << std::endl;
        // for (ui i = 0; i < count; ++i) {
        //     oss << neighbors[i] << ", ";
        // }
        // oss << std::endl;
        // LOG() << oss.str();


        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }

    void buildCoreTable();
};


#endif //SUBGRAPHMATCHING_GRAPH_H
