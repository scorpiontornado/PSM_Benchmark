#ifndef SPLITGRAPH_H 
#define SPLITGRAPH_H 

#include "graph/graph.h"
#include <queue>
#include <map>
#include <cmath>
#include <algorithm>

enum SPLITMODE {
    Q = 0,
    C = 1,
    NOSPLIT = 2,
};

enum SplitPattern {
    HYBRIDPATH_RANDOM = 0,
    TWINTWIG = 1,
    STAR = 2,
    SEED = 3,
};

enum JoinParadigm {
    LEFTDEEP = 0,
    BUSHY = 1,
};

struct SimpleStack {
    ui* stack{nullptr};
    ui tt{0};
    ui capacity{0};

    SimpleStack(ui capacity) : capacity(capacity) {
        stack = new VertexID[capacity];
    }

    ~SimpleStack() {
        delete[] stack;
    }

    void push(VertexID vid) {
        stack[tt++] = vid;
    }

    VertexID top() {
        if (!tt) {
            std::cout << "Error: stack is empty, tt == 0" << std::endl;
            abort();
        }
        return stack[tt - 1];
    }

    VertexID pop() {
        if (!tt) {
            std::cout << "Error: stack is empty, tt == 0" << std::endl;
            abort();
        }
        return stack[--tt];
    }

    bool empty() {
        return tt == 0;
    }

    int size() {
        return tt;
    }
};

struct BranchNode {
    ui node; // 当前分叉节点
    ui branch_parent; // 父分叉节点，如果没有则为UINT_MAX

    BranchNode() : node(INVALID_VERTEX_ID), branch_parent(INVALID_VERTEX_ID) {}
};

struct LeafNode {
    ui node;
    ui subgraph_id;
};

class InducedSubgraph {

  public:
    InducedSubgraph(ui vertices_count, ui lable_count)
    : total_vertices_count(vertices_count) {
        path = new ui[vertices_count];
        // NOTE: 这里的true指的是enable_vertex_label
        subgraph = new Graph(true);
        old2news = new ui[vertices_count];
        new2olds = new ui[vertices_count];
        l_old2news = new ui[lable_count];
        l_new2olds = new ui[lable_count];
    }

    ~InducedSubgraph() {
        delete[] path;
        delete subgraph;
        delete[] old2news;
        delete[] new2olds;
        delete[] l_old2news;
        delete[] l_new2olds;
    }

    ui getVerticesCount() const {
        return subgraph->getVerticesCount();
    }

    void printGraph() const {
        std::ostringstream oss;

        // 输出顶点信息
        oss << "Vertices: " << std::endl;
        for (ui i = 0; i < subgraph->getVerticesCount(); ++i) {
            oss << new2olds[i] << ": " 
                        << "labels_=" << subgraph->getVertexLabel(i)
                        << ", degree=" << subgraph->getVertexDegree(i) 
                        << std::endl;
        }

        // 输出边信息
        oss << "Edges: " << std::endl;
        for (ui i = 0; i < subgraph->getVerticesCount(); ++i) {
            oss << "u " << new2olds[i] << ": ";
            ui count;
            const VertexID* neighbors = subgraph->getVertexNeighbors(i, count);
            for (ui j = 0; j < count; ++j) {
                oss << new2olds[neighbors[j]];
                if (j < count - 1) {
                    oss << ", ";
                }
            }
            oss << std::endl;
        }

        // 一次性输出
        LOG() << oss.str();
    }

    ui total_vertices_count{0};
    ui* path;
    Graph* subgraph;
    ui* old2news;
    ui* new2olds;
    ui* l_old2news;
    ui* l_new2olds;
};

class JoinTree {
  public:
    JoinTree(ui v_count, ui r)
    : root(r), vertices_count(v_count) {
        parent.resize(v_count, INVALID_VERTEX_ID);
        // NOTE: root 节点的父亲也是 INVALID_VERTEX_ID
    }

    void printJoinTree() {
        std::ostringstream oss;
        oss << "-------------printJoinTree-------------" << std::endl;
        oss << "u_branchNodes and childs : " << std::endl;
        for (auto u : branchNodes) {
            oss << u.first << "'s parent = " << br_parent[u.first] << ", childs : ";
            for (auto& chds : childs[u.first]) {
                oss << chds << ", ";
            }
            oss << std::endl;
        }
        oss << std::endl;
        oss << "u_leafNodes : " << std::endl;
        for (auto u : leafNodes) {
            oss << u.first << "'s parent = " << br_parent[u.first] << std::endl;
        }
        oss << std::endl;
        LOG() << oss.str();
    };

    void setBranchNode(ui u) {
        // LOG() << "save Branch Node : " << u << std::endl;
        branchNodes[u].node = u;
        branchNodes[u].branch_parent = INVALID_VERTEX_ID;
    }

    void setLeafNode(ui leaf_u, ui subgraph_id) {
        // LOG() << "save Leaf Node : " << leaf_u << std::endl;
        leafNodes[leaf_u].node = leaf_u;
        leafNodes[leaf_u].subgraph_id = subgraph_id;
    }

    void setParent(ui p, ui chd) {
        parent[chd] = p;
        // childs[p].push_back(chd);
    }

    // NOTE: rootparent，不知道会不会产生错误
    void createBranchTree() {
        // // 查找u的祖先节点中最近的分叉节点
        // for (ui u = 0; u < vertices_count; ++u) {
        //     if (branchNodes.find(u) == branchNodes.end()) {
        //         continue;
        //     }
        //     BranchNode& brn = branchNodes[u];
        //     ui p = parent[u];
        //     while (p != INVALID_VERTEX_ID && branchNodes.find(p) == branchNodes.end()) {
        //         p = parent[p];
        //     }
        //     if (p != INVALID_VERTEX_ID) {
        //         brn.branch_parent = p;
        //         branchNodes[p].children.push_back(u);
        //     }
        // }

        for (auto& leaf_u: leafNodes) {
            ui p = parent[leaf_u.first];
            while (p != INVALID_VERTEX_ID && branchNodes.find(p) == branchNodes.end()) {
                p = parent[p];
            }
            if (p != INVALID_VERTEX_ID) {
                childs[p].push_back(leaf_u.first);  
                br_parent[leaf_u.first] = p;    
            } else {
                // 这个leafNode是root
                jointree_root = leaf_u.first;
            }
        }

        for (auto& brc_u : branchNodes) {
            ui p = parent[brc_u.first];
            while (p != INVALID_VERTEX_ID && branchNodes.find(p) == branchNodes.end()) {
                p = parent[p];
            }
            if (p != INVALID_VERTEX_ID) {
                childs[p].push_back(brc_u.first);
                br_parent[brc_u.first] = p;
            } else {
                // 这个branchNode是root
                jointree_root = brc_u.first;
            }
        }
        
    }

    ui root{0};
    ui vertices_count{0};
    VertexID jointree_root{0}; // jointree的root要么是branchNode，要么是leafNode（当且仅当只有一条path）

    // 这几个结构组成了sketch_tree
    std::vector<ui> parent; // 记录每个节点的父节点
    std::unordered_map<ui, std::vector<ui>> childs; // NOTE: 这个childs应该只存储 BranchNode & BranchNode 和 BranchNode & LeafNode 的父子关系
    std::unordered_map<ui, ui> br_parent; // 记录branchTree的父子关系
    std::unordered_map<ui, BranchNode> branchNodes; // 存储分叉节点及其关系
    std::unordered_map<ui, LeafNode> leafNodes; // 存储叶子节点及其关系
};

// 直接一个graph就好了，不用专门写个twintwig
// 还是自己写一个subgraph类好了，可能是inducedSubgraph的变体
class TwinTwig {
  public:
    std::vector<ui> vertices_;
    std::vector<std::vector<ui>> edges_;
    ui vertices_cnt_;
    ui edges_cnt_;

    TwinTwig() {
        vertices_cnt_ = 0;
        edges_cnt_ = 0;
        vertices_ = std::vector<ui>(0);
        edges_ = std::vector<std::vector<ui>>(0);
    }

    TwinTwig(const std::vector<ui>& vertices, const std::vector<std::pair<ui, ui>>& edges) {
        vertices_cnt_ = vertices.size();
        edges_cnt_ = edges.size();
        vertices_.resize(vertices_cnt_);
        std::copy(vertices.begin(), vertices.end(), vertices_.begin());
        std::sort(vertices_.begin(), vertices_.end());
        // 确保没有重复元素
        assert(std::adjacent_find(vertices_.begin(), vertices_.end()) == vertices_.end());

        // LOG() << "max uid == " << vertices_[vertices_.size() - 1] << std::endl;
        // 用最大的uid来赋值，每个元素都是大小为零的vector
        edges_.resize(vertices_[vertices_.size() - 1] + 1, std::vector<ui>(0));      
        // copy edgeList
        for (auto& edge : edges) {
            // LOG() << "edge.first == " << edge.first << "edge.second == " << edge.second << std::endl;
            edges_[edge.first].push_back(edge.second);
            edges_[edge.second].push_back(edge.first);
        }
        for (auto& vec : edges_) {
            std::sort(vec.begin(), vec.end());
        }
    }

    ~TwinTwig() = default;

    /*
        定义比较操作，用于 std::set 去重
        按照3个点的字典序
    */

    void expand(const TwinTwig& rhs) {
        // Step 1: Merge vertices
        std::vector<ui> merged_vertices = vertices_;
        merged_vertices.insert(merged_vertices.end(), rhs.vertices_.begin(), rhs.vertices_.end());

        // Remove duplicate vertices and sort
        std::sort(merged_vertices.begin(), merged_vertices.end());
        merged_vertices.erase(std::unique(merged_vertices.begin(), merged_vertices.end()), merged_vertices.end());

        // Update vertices and count
        vertices_ = merged_vertices;
        vertices_cnt_ = vertices_.size();

        // Step 2: Merge edges
        // Resize edges_ to accommodate new vertices
        ui max_vertex_id = vertices_.back();
        if (edges_.size() <= max_vertex_id) {
            edges_.resize(max_vertex_id + 1);
        }

        // Add edges from rhs
        for (ui u : rhs.vertices_) {
            if (u >= rhs.edges_.size()) continue;
            for (ui v : rhs.edges_[u]) {
                if (std::find(edges_[u].begin(), edges_[u].end(), v) == edges_[u].end()) {
                    edges_[u].push_back(v);
                    edges_[v].push_back(u); // Assuming undirected graph
                }
            }
        }

        // Sort edges to maintain consistency
        for (auto& edge_list : edges_) {
            std::sort(edge_list.begin(), edge_list.end());
            edge_list.erase(std::unique(edge_list.begin(), edge_list.end()), edge_list.end());
        }

        // Step 3: Update edge count
        edges_cnt_ = 0;
        for (const auto& edge_list : edges_) {
            edges_cnt_ += edge_list.size();
        }
        edges_cnt_ /= 2; // Each edge is counted twice in an undirected graph
    }

    bool operator<(const TwinTwig& rhs) const {
        // LOG() << "call < operator" << std::endl;
        if (vertices_cnt_ != rhs.vertices_cnt_) {
            return vertices_cnt_ < rhs.vertices_cnt_;
        }
        for (ui i = 0; i < vertices_cnt_; ++i) {
            if (vertices_[i] != rhs.vertices_[i]) {
                return vertices_[i] < rhs.vertices_[i];
            }

            for (ui j = 0; j < edges_[vertices_[i]].size() && j < rhs.edges_[vertices_[i]].size(); ++j) {
                if (edges_[vertices_[i]][j] != rhs.edges_[vertices_[i]][j]) {
                    return edges_[vertices_[i]][j] < rhs.edges_[vertices_[i]][j];
                }
            }
            if (edges_[vertices_[i]].size() != rhs.edges_[vertices_[i]].size()) {
                return edges_[vertices_[i]] < rhs.edges_[vertices_[i]];
            }
        }
        return false;
    }

    bool operator==(const TwinTwig& rhs) const {
        // LOG() << "call == operator" << std::endl;
        if (vertices_cnt_ != rhs.vertices_cnt_) {
            // LOG() << "not same TwinTwigs" << std::endl;
            // print();
            // rhs.print();
            return false;
        }
        /**
            按照边表的字典序排列
         */
        for (ui i = 0; i < vertices_cnt_; ++i) {
            if (vertices_[i] != rhs.vertices_[i]) {
                // LOG() << "not same TwinTwigs" << std::endl;
                // print();
                // rhs.print();
                return false;
            }
            if (edges_[vertices_[i]].size() != rhs.edges_[vertices_[i]].size()) {
                return false;
            }
            if (!std::equal(edges_[vertices_[i]].begin(), edges_[vertices_[i]].end(), rhs.edges_[vertices_[i]].begin())) {
                return false;
            }
        }
        // LOG() << "same TwinTwigs" << std::endl;
        // print();
        // rhs.print();
        return true;
    }

    bool operator!=(const TwinTwig& rhs) const {
        return !(*this == rhs);
    }

    ui getEdgesCount() const {
        return edges_cnt_;
    }
    
    ui getVerticesCount() const {
        return vertices_cnt_;
    }

    ui getVertexNbrCount(ui u) const {
        return edges_[u].size();
    }

    const ui* getVertexNeighbors(ui u) const {
        return edges_[u].data();
    }

    bool hasCommonEdges(const TwinTwig& rhs) {
        // 遍历 this 的所有边
        for (ui u : vertices_) {
            if (u >= edges_.size()) continue; // 检查索引是否越界
            for (ui v : edges_[u]) {
                // 确保边的方向一致
                ui smaller = std::min(u, v);
                ui larger = std::max(u, v);
                // LOG() << "search edge " << smaller << "->" << larger << std::endl;

                // 遍历 rhs 的所有边，寻找匹配
                if (rhs.edges_.size() > smaller) {
                    const auto& rhs_neighbors = rhs.edges_[smaller];
                    if (std::find(rhs_neighbors.begin(), rhs_neighbors.end(), larger) != rhs_neighbors.end()) {
                        // 找到相同的边
                        // LOG() << "common edge " << smaller << "->" << larger << std::endl;
                        return true;
                    } else {
                        // LOG() << "no edge " << smaller << "->" << larger << std::endl;
                    }
                }
            }
        }
        return false; // 没有找到相同的边
    }

    bool hasCommonVertices(const TwinTwig& rhs) {
        // 遍历 this 的所有顶点
        for (ui u : vertices_) {
            // 遍历 rhs 的所有顶点，寻找匹配
            if (std::find(rhs.vertices_.begin(), rhs.vertices_.end(), u) != rhs.vertices_.end()) {
                // 找到相同的顶点
                return true;
            }
        }
        return false; // 没有找到相同的顶点
    }

    bool isIncluded(const TwinTwig& rhs) const {
        // 如果等于的话就是包含了
        // if (this->operator==(rhs)) {
        //     return true;
        // }
        // 最大id要<=
        if (vertices_cnt_ > rhs.vertices_cnt_) {
            // LOG() << "vertices_cnt_ == " << vertices_cnt_ << " > rhs.vertices_cnt_" << rhs.vertices_cnt_ << std::endl;
            return false;
        }
        if (vertices_[vertices_.size() - 1] > rhs.vertices_[rhs.vertices_.size() - 1]) {
            // LOG() << "vertices_[vertices_.size() - 1] > rhs.vertices_[rhs.vertices_.size() - 1]" << std::endl;
            return false;
        }
        for (ui i = 0; i < edges_.size(); ++i) {
            // 每个点的边表要<=
            if (edges_[i].size() > rhs.edges_[i].size()) {
                // LOG() << "edges_[i].size() > rhs.edges_[i].size()" << std::endl;
                return false;
            }
            // 边表必须包含，要merge来做
            for (ui j = 0, k = 0; j < edges_[i].size(); ++j) {
                while (k < rhs.edges_.size() && edges_[i][j] > rhs.edges_[i][k]) {
                    ++k;
                }
                if (k < rhs.edges_.size() && edges_[i][j] == rhs.edges_[i][k]) {
                    continue;
                }  else {
                    // LOG() << "edges_[i][j] != rhs.edges_[i][k]" << std::endl;
                    return false;
                }
            }
        }
        return true;
    }

    void getBidirectEdgesList(std::vector<std::pair<ui, ui>>& edgeList) {
        for (ui i = 0; i < vertices_cnt_; ++i) {
            ui u = vertices_[i];
            for (ui j = 0; j < edges_[u].size(); ++j) {
                ui v = edges_[u][j];
                edgeList.push_back({u, v});
            }
        }
    }

    void getEdgesList(std::vector<std::pair<ui, ui>>& edgeList) {
        for (ui i = 0; i < vertices_cnt_; ++i) {
            ui u = vertices_[i];
            for (ui j = 0; j < edges_[u].size(); ++j) {
                ui v = edges_[u][j];
                if (u < v) {
                    edgeList.push_back({u, v});
                }
            }
        }
    }

    void getVerticesList(std::vector<ui>& verticesList) {
        for (ui i = 0; i < vertices_cnt_; ++i) {
            verticesList.push_back(vertices_[i]);
        }
    }

    void print() const {
        std::ostringstream oss;
        oss << "TwinTwig: " << std::endl;
        oss << "Vertices count " << vertices_cnt_ << " : ";
        for (ui i = 0; i < vertices_cnt_; ++i) {
            oss << vertices_[i] << ", ";
        }
        oss << std::endl;
        oss << "Edges count " << edges_.size() << ": ";
        for (ui i = 0; i < edges_.size(); ++i) {
            oss << i << " -> : ";
            for (ui j = 0; j < edges_[i].size(); ++j) {
                oss << edges_[i][j] << ", ";
            }
            oss << std::endl;
        }
        LOG() << oss.str();
    }
};

namespace std {
    template <>
    struct hash<TwinTwig> {
        size_t operator()(const TwinTwig& tt) const {
            size_t hash_value = 0;
            // LOG() << "call hash TwinTwig" << std::endl;

            // 对 vertices_ 进行哈希
            for (const auto& vertex : tt.vertices_) {
                hash_value = hash_value * 31 + std::hash<ui>()(vertex);
            }

            // 对 edges_ 进行哈希
            for (size_t i = 0; i < tt.edges_.size(); ++i) {
                auto& vec = tt.edges_[i];
                if (vec.size() > 0) {
                    ui a = i;
                    for (size_t j = 0; j < vec.size(); ++j) {
                        ui b = vec[j];
                        // 确保无序对 (a, b) 和 (b, a) 产生相同的哈希值
                        // std::tie(a, b) = std::minmax(a, b); // 确保 a <= b
                        hash_value = hash_value * 31 + std::hash<ui>()(a);
                        hash_value = hash_value * 31 + std::hash<ui>()(b);
                    }
                }
            }
            return hash_value;
        }
    };
}

struct State {
    double costValue{0.0};
    double traceCost{0.0};
    double deltaCost{0.0};
    std::vector<ui> twIds = std::vector<ui>(0);
};

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        std::size_t hash = 0;
        // 计算第一个元素的哈希值
        hash ^= std::hash<T1>()(p.first) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        // 计算第二个元素的哈希值
        hash ^= std::hash<T2>()(p.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

// 定义自定义哈希函数
struct TupleHash4 {
    std::size_t operator()(const std::tuple<ui, ui, ui, ui>& tuple) const {
        std::size_t hash = 0;
        hash ^= std::hash<ui>()(std::get<0>(tuple)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<ui>()(std::get<1>(tuple)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<ui>()(std::get<2>(tuple)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<ui>()(std::get<3>(tuple)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

class JoinSequence {
  public:
    JoinSequence(ui v_count)
    : vertices_count(v_count) {}

    ui vertices_count{0};
    ui unit_cnt{0};

    std::unordered_map<std::tuple<ui, ui, ui, ui>, double, TupleHash4> deltaCosts;
    TwinTwig destPattern;
};

class SplitGraph {
  public:    
    SplitGraph(Graph* d_graph, Graph* graph, JoinParadigm join_paradigm, SplitPattern split_pattern, SPLITMODE split_mode, int64_t time_limit)
    : d_graph(d_graph), graph(graph), join_paradigm(join_paradigm), split_pattern(split_pattern), split_mode(split_mode), time_limit(time_limit),
      starpq(&SplitGraph::compareStar), twintwigpq(&SplitGraph::compareTwinTwig) {
        stk = new SimpleStack(graph->getVerticesCount());
        visited = new bool[graph->getVerticesCount()]();
        std::fill(visited, visited + graph->getVerticesCount(), false);
        if (split_mode == SPLITMODE::NOSPLIT || split_mode == SPLITMODE::C) {
            // if (split_pattern == SplitPattern::HYBRIDPATH_RANDOM) {
                // get a u_args the same as g_args
                // TODO : if need not split Q, then use g_args, not u_args, so we don't need loadSubgFromGraph
                // TODO : the leaf, root, branchRoot is not correct now
            std::vector<VertexID> path;
            for (ui i = 0; i < graph->getVerticesCount(); ++i) {
                path.push_back(i);
            }
            saveSubgraph(path);
            subgraph_count++;
            jtree = new JoinTree(graph->getVerticesCount(), 0);
            // jtree->setBranchNode(0);
            jtree->setLeafNode(graph->getVerticesCount() -  1, 0);
            // jtree->setParent(0, graph->getVerticesCount() -  1);
            jtree->createBranchTree();
            return;
            // }
        } else if (split_mode == SPLITMODE::Q) {
            splitgraph(); // 这一步要动态确定 subgraphs.size()
        }
        // LOG() << "split Over" << std::endl;
    }

    ~SplitGraph() {
        delete[] visited;
        delete stk;
        for (int i = 0; i < subgraphs.size(); ++i) {
            delete subgraphs[i];
        }
        delete jtree;
        delete jseq;
    }

    std::vector<InducedSubgraph*> getSubgraphs();

    int getSubgraphCount();

  public:
    void printSketchTree();
    void printPaths();

  private:
    void updateVisitedNbrCnt(std::vector<ui>& visited_nbr_cnt, ui u);
    void saveSubgraph(std::vector<TwinTwig>& tws);
    void saveSubgraph(std::vector<ui>& path);
    void addBranchNode();

    void splitgraph();

    void split_hybrid_rnd();
    void split_hybrid_dmax();
    void split_hybrid_ascdmax();

    void constructAllTwinTwigs();
    void printAllTwinTwigs();
    void initAllCosts();
    
    void printInitCosts();
    bool concateable(TwinTwig tw1, TwinTwig tw2);
    TwinTwig concatenate(TwinTwig tw1, TwinTwig tw2);
    TwinTwig getDestPattern();
    void updateCommonCount(TwinTwig& currentSubgraph, std::vector<TwinTwig>& stars, std::unordered_map<TwinTwig, bool>& starAdded);
    std::vector<TwinTwig> greedyTwinTwigDecomposition();
    std::vector<TwinTwig> greedyStarDecomposition();
    std::vector<TwinTwig> optimalDecomposition();
    double card(ui m, ui n);
    // 需要加入 twintwig 结构和decomposition_Unit两种结构
    // 所以不用 twintwig，直接用 subgraph，但是要加hashfunction，这里不是inducedSubgraph，而是Subgraph
    void split_twintwig();
    void split_star();
    void split_seed();

    bool checkOverTime();
    int64_t time_limit;

  public:
    // JoinTree里面branchNode之类的结构
    JoinTree* jtree{nullptr};
    JoinSequence* jseq{nullptr};

  private:
    Graph* d_graph{nullptr};
    Graph* graph{nullptr};
    JoinParadigm join_paradigm{LEFTDEEP};
    SplitPattern split_pattern{HYBRIDPATH_RANDOM};

    // 需要在这加上decomposition/join planner的东西
    // JoinPlanNode** joinPlanTree{nullptr};

    // q_graph : id {0, 1, ..., K}
    // coresponding to the path[0~K-1], subgraph[0~K-1], old2new[0~K-1], new2old[0~K-1], trees[0~K-1]
    int subgraph_count{0};
    bool* visited{nullptr};
    SimpleStack* stk;
    std::vector<InducedSubgraph*> subgraphs;

    SPLITMODE split_mode;

    // ui 1大的在前，ui 2小的在前，tw边数小的在前
    static bool compareStar(const std::tuple<TwinTwig, ui, ui>& a, const std::tuple<TwinTwig, ui, ui>& b) {
        if (std::get<1>(a) != std::get<1>(b))
            return std::get<1>(a) > std::get<1>(b);
        if (std::get<2>(a) != std::get<2>(b))
            return std::get<2>(a) < std::get<2>(b);
        if (std::get<0>(a).getEdgesCount() != std::get<0>(b).getEdgesCount()) {
            return std::get<0>(a).getEdgesCount() < std::get<0>(b).getEdgesCount();
        }
        return std::get<0>(a) < std::get<0>(b);
    }

    std::set<std::tuple<TwinTwig, ui, ui>, decltype(&SplitGraph::compareStar)> starpq;

    // 按照get<1>的从小到大排序，costValue，然后是边越多的越靠前
    static bool compareTwinTwig(const std::tuple<TwinTwig, ui, ui, std::vector<TwinTwig>>& a, const std::tuple<TwinTwig, ui, ui, std::vector<TwinTwig>>& b) {
        if (std::get<1>(a) != std::get<1>(b))
            return std::get<1>(a) < std::get<1>(b);
        return std::get<0>(a).getEdgesCount() > std::get<0>(b).getEdgesCount();
    }

    // static bool compareTwinTwig(const std::tuple<TwinTwig, ui, ui, std::vector<TwinTwig>>& a, const std::tuple<TwinTwig, ui, ui, std::vector<TwinTwig>>& b) {
    //     if (std::get<1>(a) != std::get<1>(b))
    //         return std::get<1>(a) < std::get<1>(b);
    //     if (std::get<0>(a).getEdgesCount() != std::get<0>(b).getEdgesCount()) {
    //         return std::get<0>(a).getEdgesCount() > std::get<0>(b).getEdgesCount();    
    //     }
    //     return std::get<0>(b) < std::get<0>(a);
    // }
    // std::vector<TwinTwig>是D = {p1, p2, ... , pk}
    std::set<std::tuple<TwinTwig, ui, ui, std::vector<TwinTwig>>, decltype(&SplitGraph::compareTwinTwig)> twintwigpq;
};

#endif // SPLITGRAPH_H