#include "graphoperations.h"
#include <memory.h>
#include <queue>

void GraphOperations::getKCore(const Graph *graph, int *core_table) {
    int vertices_count = graph->getVerticesCount();
    int max_degree = graph->getGraphMaxDegree();

    int* vertices = new int[vertices_count];          // Vertices sorted by degree.
    int* position = new int[vertices_count];          // The position of vertices in vertices array.
    int* degree_bin = new int[max_degree + 1];      // Degree from 0 to max_degree.
    int* offset = new int[max_degree + 1];          // The offset in vertices array according to degree.

    std::fill(degree_bin, degree_bin + (max_degree + 1), 0);

    for (int i = 0; i < vertices_count; ++i) {
        int degree = graph->getVertexDegree(i);
        core_table[i] = degree;
        degree_bin[degree] += 1;
    }

    int start = 0;
    for (int i = 0; i < max_degree + 1; ++i) {
        offset[i] = start;
        start += degree_bin[i];
    }

    for (int i = 0; i < vertices_count; ++i) {
        int degree = graph->getVertexDegree(i);
        position[i] = offset[degree];
        vertices[position[i]] = i;
        offset[degree] += 1;
    }

    for (int i = max_degree; i > 0; --i) {
        offset[i] = offset[i - 1];
    }
    offset[0] = 0;

    for (int i = 0; i < vertices_count; ++i) {
        int v = vertices[i];

        ui count;
        const VertexID * neighbors = graph->getVertexNeighbors(v, count);

        for(int j = 0; j < count; ++j) {
            int u = neighbors[j];

            if (core_table[u] > core_table[v]) {

                // Get the position and vertex which is with the same degree
                // and at the start position of vertices array.
                int cur_degree_u = core_table[u];
                int position_u = position[u];
                int position_w = offset[cur_degree_u];
                int w = vertices[position_w];

                if (u != w) {
                    // Swap u and w.
                    position[u] = position_w;
                    position[w] = position_u;
                    vertices[position_u] = w;
                    vertices[position_w] = u;
                }

                offset[cur_degree_u] += 1;
                core_table[u] -= 1;
            }
        }
    }

    delete[] vertices;
    delete[] position;
    delete[] degree_bin;
    delete[] offset;
}

void GraphOperations::old_cheap(int* col_ptrs, int* col_ids, int* match, int* row_match, int n, int m) {
    int ptr;
    int i = 0;
    for(; i < n; i++) {
        int s_ptr = col_ptrs[i];
        int e_ptr = col_ptrs[i + 1];
        for(ptr = s_ptr; ptr < e_ptr; ptr++) {
            int r_id = col_ids[ptr];
            if(row_match[r_id] == -1) {
                match[i] = r_id;
                row_match[r_id] = i;
                break;
            }
        }
    }
}

void GraphOperations::match_bfs(int* col_ptrs, int* col_ids, int* match, int* row_match, int* visited,
                        int* queue, int* previous, int n, int m) {
    int queue_ptr, queue_col, ptr, next_augment_no, i, j, queue_size,
            row, col, temp, eptr;

    old_cheap(col_ptrs, col_ids, match, row_match, n, m);

    memset(visited, 0, sizeof(int) * m);

    next_augment_no = 1;
    for(i = 0; i < n; i++) {
        if(match[i] == -1 && col_ptrs[i] != col_ptrs[i+1]) {
            queue[0] = i; queue_ptr = 0; queue_size = 1;

            while(queue_size > queue_ptr) {
                queue_col = queue[queue_ptr++];
                eptr = col_ptrs[queue_col + 1];
                for(ptr = col_ptrs[queue_col]; ptr < eptr; ptr++) {
                    row = col_ids[ptr];
                    temp = visited[row];

                    if(temp != next_augment_no && temp != -1) {
                        previous[row] = queue_col;
                        visited[row] = next_augment_no;

                        col = row_match[row];

                        if(col == -1) {
                            // Find an augmenting path. Then, trace back and modify the augmenting path.
                            while(row != -1) {
                                col = previous[row];
                                temp = match[col];
                                match[col] = row;
                                row_match[row] = col;
                                row = temp;
                            }
                            next_augment_no++;
                            queue_size = 0;
                            break;
                        } else {
                            // Continue to construct the match.
                            queue[queue_size++] = col;
                        }
                    }
                }
            }

            if(match[i] == -1) {
                for(j = 1; j < queue_size; j++) {
                    visited[match[queue[j]]] = -1;
                }
            }
        }
    }
}

/*
    这里是把每个点的parent, children, level都记录下来
    但是bn_还有很多没记录的，只记录了搜索下来的唯一的parent，fn_同理
    这里已经把order给确定下来了，不过仅仅是dag的order(sibling的order应该是定死了)，可能还会有别的分支方法
*/
void GraphOperations::bfsTraversal(const Graph *graph, VertexID root_vertex, TreeNode *&tree, VertexID *&bfs_order) {
    ui vertex_num = graph->getVerticesCount();

    std::queue<VertexID> bfs_queue;
    std::vector<bool> visited(vertex_num, false);

    // tree = new TreeNode[vertex_num];
    // LOG() << "tree[i].size == " << vertex_num << std::endl;
    for (ui i = 0; i < vertex_num; ++i) {
        tree[i].initialize(vertex_num);
    }
    // bfs_order = new VertexID[vertex_num];

    ui visited_vertex_count = 0;
    bfs_queue.push(root_vertex);
    visited[root_vertex] = true;
    tree[root_vertex].level_ = 0;
    tree[root_vertex].id_ = root_vertex;

    while(!bfs_queue.empty()) {
        const VertexID u = bfs_queue.front();
        bfs_queue.pop();
        bfs_order[visited_vertex_count++] = u;

        ui u_nbrs_count;
        const VertexID* u_nbrs = graph->getVertexNeighbors(u, u_nbrs_count);
        for (ui i = 0; i < u_nbrs_count; ++i) {
            VertexID u_nbr = u_nbrs[i];

            if (!visited[u_nbr]) {
                bfs_queue.push(u_nbr);
                visited[u_nbr] = true;
                tree[u_nbr].id_ = u_nbr;
                tree[u_nbr].parent_ = u;
                tree[u_nbr].level_ = tree[u] .level_ + 1;
                tree[u].children_[tree[u].children_count_++] = u_nbr;
            }
        }
    }
}

void GraphOperations::bfsTraversalOrdered(const Graph *graph, VertexID root_vertex, TreeNode *&tree,
                                            VertexID *&bfs_order, ui* order) {

    ui vertex_num = graph->getVerticesCount();

    std::vector<ui> order_index(vertex_num);
    for (ui i = 0; i < vertex_num; ++i) {
        order_index[order[i]] = i;
    }

    std::queue<VertexID> bfs_queue;
    std::vector<bool> visited(vertex_num, false);
    
    for (ui i = 0; i < vertex_num; ++i) {
        tree[i].initialize(vertex_num);
    }

    ui visited_vertex_count = 0;
    bfs_queue.push(root_vertex);
    visited[root_vertex] = true;
    tree[root_vertex].level_ = 0;
    tree[root_vertex].id_ = root_vertex;

    while (!bfs_queue.empty()) {
        VertexID u = bfs_queue.front();
        bfs_queue.pop();
        bfs_order[visited_vertex_count++] = u;

        ui u_nbrs_count;
        const VertexID* u_nbrs = graph->getVertexNeighbors(u, u_nbrs_count);

        // **核心修改：按 `order[]` 顺序排列邻居**
        std::vector<VertexID> sorted_neighbors;
        for (ui i = 0; i < u_nbrs_count; ++i) {
            VertexID u_nbr = u_nbrs[i];
            if (!visited[u_nbr]) {
                sorted_neighbors.push_back(u_nbr);
            }
        }
        std::sort(sorted_neighbors.begin(), sorted_neighbors.end(),
                  [&](VertexID a, VertexID b) { return order_index[a] < order_index[b]; });

        // **按 `order[]` 规定的顺序进行 BFS**
        for (VertexID u_nbr : sorted_neighbors) {
            if (!visited[u_nbr]) {
                bfs_queue.push(u_nbr);
                visited[u_nbr] = true;
                tree[u_nbr].id_ = u_nbr;
                tree[u_nbr].parent_ = u;
                tree[u_nbr].level_ = tree[u].level_ + 1;
                tree[u].children_[tree[u].children_count_++] = u_nbr;
            }
        }
    }
}

void GraphOperations::printBFSTree(TreeNode* tree, VertexID vertex_num) {
    std::ostringstream oss;
    oss << "----------printBFSTree----------" << std::endl;

    // 遍历所有节点，按照层次输出
    for (ui i = 0; i < vertex_num; ++i) {
        oss << "Vertex: " << tree[i].id_
            << " | Parent: " << (tree[i].parent_ == INVALID_VERTEX_ID ? -1 : tree[i].parent_)
            << " | Level: " << tree[i].level_
            << " | Children: ";

        // 打印子节点
        for (ui j = 0; j < tree[i].children_count_; ++j) {
            oss << tree[i].children_[j] << " ";
        }
        oss << std::endl;
    }

    // 输出结果
    LOG() << oss.str();
}


void GraphOperations::dfsTraversal(TreeNode *tree, VertexID root_vertex, ui node_num, VertexID *&dfs_order) {
    dfs_order = new VertexID[node_num];
    ui count = 0;
    dfs(tree, root_vertex, dfs_order, count);
}

void GraphOperations::dfs(TreeNode *tree, VertexID cur_vertex, VertexID *dfs_order, ui &count) {
    dfs_order[count++] = cur_vertex;

    for (ui i = 0; i < tree[cur_vertex].children_count_; ++i) {
        dfs(tree, tree[cur_vertex].children_[i], dfs_order, count);
    }
}
