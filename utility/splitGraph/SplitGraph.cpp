#include "SplitGraph.h"
#include "utility/computesetintersection.h"
#include "utility/statistics/intTimer.h"

bool
SplitGraph::checkOverTime() {
    if (intTimer::getClockNano() >= time_limit) {
        return true;
    }
    return false;
}

/*
    计算 card 值，即给定m,n，计算他在M和N下的匹配数量
*/
double
SplitGraph::card(ui m, ui n) {
    ui M = d_graph->getEdgesCount();
    ui N = d_graph->getVerticesCount();
    return pow(2 * M, m) / pow(N, 2 * m - n);
}

void SplitGraph::splitgraph() {
    if (split_pattern == HYBRIDPATH_RANDOM) {
        split_hybrid_rnd();
    } else if (split_pattern == TWINTWIG) {
        split_twintwig();
    } else if (split_pattern == STAR) {
        split_star();
    } else if (split_pattern == SEED) {
        split_seed();
    } else {
        LOG() << "Error: split pattern is not defined." << std::endl;
        abort();
    }
}

void SplitGraph::split_hybrid_rnd() {
    ui root = graph->getMaxDegreeVid();
    visited[root] = true;
    jtree = new JoinTree(graph->getVerticesCount(), root);
    ui u_nbrs_count;

    stk->push(root);
    std::vector<ui> path; // 用于记录当前路径
    path.push_back(root); // 将根节点添加到路径中

    // 用于记录上次这个点到了第几个邻居，加速一下
    std::vector<ui> idx(graph->getVerticesCount(), 0);

    std::unordered_map<ui, bool> isLeaf;
    for (ui i = 0; i < graph->getVerticesCount(); ++i) {
        isLeaf[i] = true;
    }

    while (!stk->empty()) {
        if (checkOverTime()) {
            return;
        }
        ui u = stk->top();
        const ui* u_nbrs = graph->getVertexNeighbors(u, u_nbrs_count);
        bool hasChild = false;
        // LOG() << "top = u : " << u << std::endl;

        for (ui i = idx[u]; i < u_nbrs_count; ++i) {
            ui v = u_nbrs[i];
            if (visited[v]) {
                idx[u]++;
                continue;
            }
            visited[v] = true;
            jtree->setParent(u, v); // 记录父节点
            isLeaf[u] = false;

            stk->push(v);
            path.push_back(v);
            idx[u]++;
            hasChild = true;
            break;
        }

        if (hasChild) {
            continue;
        }

        // LOG() << "to be poped u = " << u << std::endl;
        // LOG() << "isLeaf[u] == " << (isLeaf[u] ? "Yes" : "No") << std::endl;
        if (!isLeaf[u]) {
            stk->pop();
            path.pop_back();
            addBranchNode();
        } else if (isLeaf[u]) {
            saveSubgraph(path);
            /*
                保留leaf节点对应的subgraph_id
                因为leaf节点与path一一对应，path与subgraph一一对应
                要用subgraph里保存的new2old和old2new来还原原图
            */ 

            ui leaf = stk->top();
            jtree->setLeafNode(leaf, subgraph_count);
            subgraph_count++;
 
            path.pop_back(); // 从路径中移除当前节点
            stk->pop();      // 弹出栈顶元素
            addBranchNode();
        }
    }
    jtree->createBranchTree();
}

void SplitGraph::printSketchTree() {
    jtree->printJoinTree();
}

// TODO: 逻辑是错的，需要根据rnd重新修改一下
void SplitGraph::split_hybrid_dmax() {
    ui root = graph->getMaxDegreeVid();
    visited[root] = true;
    jtree = new JoinTree(graph->getVerticesCount(), root);

    stk->push(root);
    std::vector<ui> path; // 用于记录当前路径
    path.push_back(root); // 将根节点添加到路径中

    // 用于记录上次这个点到了第几个邻居，加速一下
    std::vector<ui> idx(graph->getVerticesCount(), 0);

    // 存储每个节点的已排序邻居列表
    std::vector<std::vector<ui>> sorted_neighbors(graph->getVerticesCount());

    while (!stk->empty()) {
        ui u = stk->top(); // 获取栈顶元素，但不弹出
        bool overPath = true;
        ui neighbor_count = 0;
        const ui* neighbors = graph->getVertexNeighbors(u, neighbor_count);

        // 如果还没有为节点u排序过邻居，则进行排序
        if (sorted_neighbors[u].empty()) {
            // 将邻居复制到向量中
            std::vector<ui> nbrs(neighbors, neighbors + neighbor_count);
            // 按度数降序排序邻居
            std::sort(nbrs.begin(), nbrs.end(), [&](ui a, ui b) {
                return graph->getVertexDegree(a) > graph->getVertexDegree(b);
            });
            // 保存排序后的邻居列表
            sorted_neighbors[u] = nbrs;
        }

        std::vector<ui>& u_sorted_nbrs = sorted_neighbors[u];

        ui& i = idx[u]; // 引用 idx[u]，方便直接修改
        for (; i < u_sorted_nbrs.size(); ++i) {
            ui v = u_sorted_nbrs[i];
            if (visited[v]) {
                continue;
            }
            visited[v] = true;
            stk->push(v);
            path.push_back(v);
            overPath = false;
            i++; // 更新 idx[u]
            break; // 只选取度数最大的未访问邻居
        }

        if (overPath) {
            saveSubgraph(path);

            ui leaf = stk->top();

            jtree->setLeafNode(leaf, subgraph_count++);

            path.pop_back(); // 从路径中移除当前节点
            stk->pop();      // 弹出栈顶元素
            addBranchNode();
        }
    }
    jtree->createBranchTree();
}

void
SplitGraph::split_hybrid_ascdmax() {
    ui root = graph->getMaxDegreeVid();
    visited[root] = true;
    jtree = new JoinTree(graph->getVerticesCount(), root);

    // 初始化每个节点的已访问邻居计数
    std::vector<ui> visited_nbr_cnt(graph->getVerticesCount(), 0);
    updateVisitedNbrCnt(visited_nbr_cnt, root);

    stk->push(root);
    std::vector<ui> path; // 用于记录当前路径
    path.push_back(root); // 将根节点添加到路径中

    std::unordered_map<ui, bool> isLeaf;
    for (ui i = 0; i < graph->getVerticesCount(); ++i) {
        isLeaf[i] = true;
    }

    while (!stk->empty()) {
        ui u = stk->top(); // 获取栈顶元素，但不弹出

        ui neighbor_count = 0;
        const ui* neighbors = graph->getVertexNeighbors(u, neighbor_count);

        // 收集未访问的邻居
        std::vector<ui> unvisited_neighbors;
        for (ui i = 0; i < neighbor_count; ++i) {
            ui v = neighbors[i];
            if (!visited[v]) {
                unvisited_neighbors.push_back(v);
            }
        }

        if (!unvisited_neighbors.empty()) {
            isLeaf[u] = false;
            // if (u == 10) LOG() << "u10 == false " << std::endl;
            // 在未访问的邻居中，选择已访问邻居数量最多的节点
            ui max_v = unvisited_neighbors[0];
            ui max_cnt = visited_nbr_cnt[max_v];

            for (ui i = 1; i < unvisited_neighbors.size(); ++i) {
                ui v = unvisited_neighbors[i];
                if (visited_nbr_cnt[v] > max_cnt) {
                    max_v = v;
                    max_cnt = visited_nbr_cnt[v];
                }
            }

            // 标记选中的节点为已访问
            visited[max_v] = true;
            updateVisitedNbrCnt(visited_nbr_cnt, max_v);

            stk->push(max_v);
            path.push_back(max_v);
            continue;
        }

        if (isLeaf[u]) {
            saveSubgraph(path);

            ui leaf = stk->top();
            jtree->setLeafNode(leaf, subgraph_count++);

            path.pop_back(); // 从路径中移除当前节点
            stk->pop();      // 弹出栈顶元素
            addBranchNode();
        }
    }
    jtree->createBranchTree();
}

void
SplitGraph::constructAllTwinTwigs() {
    // ui t = 0;

    // LOG() << "t == " << t << std::endl;
}

// void 
// SplitGraph::initAllCosts() {
//     auto start = std::chrono::steady_clock::now();
//     /*
//         f(n) = g(n) + h(n)
//         Costs: Cost(D_i, P) = f(n)
//         deltaCosts: Cost(D_i) = g(n)
//         deltaCosts: Delta_Cost(D_i, P) = h(n)
//     */
//     ui qm = graph->getEdgesCount();
//     ui qn = graph->getVerticesCount();
//     ui M = d_graph->getEdgesCount();

//     /*
//         初始化 deltaCosts
//         不同的Pi({m, n}对)之间是没有递推关系的
//     */
//     auto& deltaCosts = jseq->deltaCosts;
//     for (const auto& [twinTwig, _] : jseq->unitTwinTwigs) {
//         deltaCosts[std::tuple(twinTwig.getEdgesCount(), twinTwig.getVerticesCount(), 0, 0)] = 0;
//     }
//     for (ui m = 1; m < graph->getEdgesCount(); ++m) {
//         for (ui n = 2; n < graph->getVerticesCount(); ++n) {
//             deltaCosts[std::tuple(m, n, 0, 0)] = 0;
//             for (ui delta_m = 1; delta_m <= graph->getEdgesCount() - m; ++delta_m) {
//                 for (ui delta_n = 2; delta_n < graph->getVerticesCount(); ++delta_n) {
//                     // 要找最小值
//                     deltaCosts[std::tuple(m, n, delta_m, delta_n)] = std::numeric_limits<double>::max();
//                     for (ui a = 1; a <= 2; ++a) {
//                         for (ui b = 0; b <= a; ++b) {
//                             // 要确保 a <= delta_m && b <= delta_n
//                             if (a > delta_m || b > delta_n) {
//                                 continue;
//                             }
//                             double cost = deltaCosts[std::tuple(m, n, delta_m - a, delta_n - b)]
//                                         + 3 * card(m + delta_m, n + delta_n) + card(a, b) + M;
//                             deltaCosts[std::tuple(m, n, delta_m, delta_n)] = std::min(deltaCosts[std::tuple(m, n, delta_m, delta_n)], cost);
//                         }
//                     }
//                 }
//             }
//         }
//     }

//     auto end = std::chrono::steady_clock::now();
//     double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
//     LOG() << "initAllCosts_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;
// }

// void
// SplitGraph::printInitCosts() {
//     auto& deltaCosts = jseq->deltaCosts;
//     LOG() << "deltaCost'size : " << deltaCosts.size() << std::endl;
//     // for (const auto& [key, value] : deltaCosts) {
//     //     LOG() << "m, n, dm, dn : (" << std::get<0>(key) << ", " << std::get<1>(key) << ", " << std::get<2>(key) << ", " << std::get<3>(key) << "), deltaCost : " << value << std::endl;
//     // }
//     for (auto& [twinTwig, state] : jseq->unitTwinTwigs) {
//         LOG() << "state.traceCost == " << state.traceCost << std::endl;
//     }
// }

/**
    (1) 保证这两个都是isIncluded(destPattern)
    (2) 保证tw1和tw2之间有共同的点，没有共同的边
 */
bool
SplitGraph::concateable(TwinTwig tw1, TwinTwig tw2) {
    if (tw1 == tw2) {
        // LOG() << "tw1 == tw2" << std::endl;
        return false;
    }
    if (!(tw1.isIncluded(jseq->destPattern) && tw2.isIncluded(jseq->destPattern))) {
        // LOG() << "not included destPattern" << std::endl;
        return false;
    }
    ui intSecCount = 0;
    // LOG() << "tw1.vertices_.size() == " << tw1.vertices_.size() << ", tw2.vertices_.size() == " << tw2.vertices_.size() << std::endl;
    // merge to intersect
    for(ui i = 0, j = 0; i < tw1.vertices_.size(); ++i) {
        ui u = tw1.vertices_[i];
        while (j < tw2.vertices_.size() && tw2.vertices_[j] < u) {
            j++;
        }
        if (j < tw2.vertices_.size() && tw2.vertices_[j] == u) {
            intSecCount++;
        }
    }
    if (intSecCount == 0) {
        // LOG() << "no same vertices" << std::endl;
        return false;
    }
    for (ui i = 0; i < tw1.vertices_.size(); ++i) {
        ui u = tw1.vertices_[i];
        if (std::find(tw2.vertices_.begin(), tw2.vertices_.end(), u) != tw2.vertices_.end()) {
            std::vector<ui>& u_edges = tw1.edges_[u];
            std::vector<ui>& u_edges2 = tw2.edges_[u];
            // compare the 2 vectors, if there are any same edge, return false
            std::unordered_set<ui> edges_set(u_edges.begin(), u_edges.end());
            for (const auto& edge : u_edges2) {
                if (edges_set.count(edge)) {
                    // LOG() << "same edge in tw1 and tw2" << std::endl;
                    return false;
                }
            }
        }
    }
    return true;
}

/*
    NOTE: 在连接之前要手动调用 concateable 确保点有交集，并且边不会有交集
    这里就是把点集和边集合并并且去重
*/
TwinTwig
SplitGraph::concatenate(TwinTwig tw1, TwinTwig tw2) {
    std::vector<ui>& v1s = tw1.vertices_;
    std::vector<ui>& v2s = tw2.vertices_;
    std::unordered_set<ui> vs = std::unordered_set<ui>(v1s.begin(), v1s.end());
    for (const auto& v : v2s) {
        vs.insert(v);
    }
    std::vector<std::pair<ui, ui>> e1s;
    std::vector<std::pair<ui, ui>> e2s;
    tw1.getEdgesList(e1s);
    tw2.getEdgesList(e2s);
    // 使用 lambda 表达式定义自定义的 hash 函数
    auto pair_hash = [](const std::pair<ui, ui>& pair) {
        return std::hash<ui>()(pair.first) ^ (std::hash<ui>()(pair.second) << 1);
    };
    std::unordered_set<std::pair<ui, ui>, decltype(pair_hash)> es = std::unordered_set<std::pair<ui, ui>, decltype(pair_hash)>(e1s.begin(), e1s.end());
    for (const auto& e : e2s) {
        es.insert(e);
    }
    return TwinTwig(std::vector<ui>(vs.begin(), vs.end()), std::vector<std::pair<ui, ui>>(es.begin(), es.end()));
}

TwinTwig
SplitGraph::getDestPattern() {
    std::vector<ui> vertices;
    std::vector<std::pair<ui, ui>> edges;
    for (ui i = 0; i < graph->getVerticesCount(); ++i) {
        vertices.push_back(i);
        ui nbr_cnt;
        const ui* nbrs = graph->getVertexNeighbors(i, nbr_cnt);
        for (ui j = 0; j < nbr_cnt; ++j) {
            if (nbrs[j] > i) {
                edges.push_back({i, nbrs[j]});
            }
        }
    }
    return TwinTwig(vertices, edges);
}

void
SplitGraph::updateCommonCount(
        TwinTwig& currentSubgraph, std::vector<TwinTwig>& stars, std::unordered_map<TwinTwig, bool>& starAdded) {
    // 更新交的 点/边数量
    for (auto& star : stars) {
        if (starAdded[star]) {
            continue;
        }
        std::vector<ui> vs, vsstar;
        std::vector<std::pair<ui, ui>> es, esstar;
        currentSubgraph.getVerticesList(vs);
        star.getVerticesList(vsstar);
        currentSubgraph.getBidirectEdgesList(es);
        star.getBidirectEdgesList(esstar);
        // 求点的交集数量和边的交集数量
        ui commonVertexCount = 0;
        ui commonEdgeCount = 0;
        for (ui i = 0; i < vs.size(); ++i) {
            if (std::find(vsstar.begin(), vsstar.end(), vs[i]) != vsstar.end()) {
                commonVertexCount++;
            }
        }
        for (ui i = 0; i < es.size(); ++i) {
            if (std::find(esstar.begin(), esstar.end(), es[i]) != esstar.end()) {
                commonEdgeCount++;
            }
        }
        for (auto it = starpq.begin(); it != starpq.end(); ++it) {
            if (std::get<0>(*it) == star) {
                starpq.erase(it);
                break;
            }
        }
        starpq.insert({star, commonVertexCount, commonEdgeCount});
    }
}


// // 在optimal加上unitTw visited基础上，再去掉deltaCost
// std::vector<TwinTwig>
// SplitGraph::greedyTwinTwigDecomposition() {
//     std::vector<TwinTwig> twintwigs;
//     // 1. 构造所有twintwig
//     for (ui u = 0; u < graph->getVerticesCount(); ++u) {
//         ui nbr_cnt = 0;
//         const ui* nbrs = graph->getVertexNeighbors(u, nbr_cnt);
//         for (ui j = 0; j < nbr_cnt; ++j) {
//             VertexID v = nbrs[j];            
//             twintwigs.emplace_back(std::vector<ui>{u, v}, std::vector<std::pair<ui, ui>>{{u, v}});

//             // // NOTE: 这里一开始20-27-24和24-27-20会重复加入，后面edge.sort就没问题了
//             // ui v_nbr_cnt = 0;
//             // const ui* v_nbrs = graph->getVertexNeighbors(v, v_nbr_cnt);
//             // for (ui k = 0; k < v_nbr_cnt; ++k) {
//             //     VertexID w = v_nbrs[k];
//             //     if (w == u) {
//             //         continue;
//             //     }
//             //     t++;
//             //     // 以v为root的双枝结构
//             //     twinTwigs[TwinTwig({v, u, w}, {{v, u}, {v, w}})] = State(0.0, 0.0, 0.0);
//             // }
//         }
//     }
//     for (ui u = 0; u < graph->getVerticesCount(); ++u) {
//         ui nbr_cnt = 0;
//         const ui* nbrs = graph->getVertexNeighbors(u, nbr_cnt);
//         for (ui j = 0; j < nbr_cnt; ++j) {
//             for (ui k = j + 1; k < nbr_cnt; ++k) {
//                 VertexID v = nbrs[j];
//                 VertexID w = nbrs[k];
//                 twintwigs.emplace_back(std::vector<ui>{u, v, w}, std::vector<std::pair<ui, ui>>{{u, v}, {u, w}});
//             }
//         }
//     }


//     // 2. twintwigs和star的区别是前者没有重的边
//     TwinTwig currentSubgraph;
//     std::vector<TwinTwig> orderedTwintwigs;
    
//     std::unordered_map<TwinTwig, bool> starAdded;
//     // 初始化pq，把所有p_i都压进去
//     for (auto& twintwig : twintwigs) {
//         pq.insert(std::make_tuple(twintwig, 0, 0));
//         starAdded[twintwig] = false;
//     }

//     auto it = pq.begin();
//     TwinTwig st = std::get<0>(*it);
//     pq.erase(it);

//     orderedTwintwigs.push_back(st);
//     currentSubgraph = st;
//     starAdded[st] = true;
//     updateCommonCount(currentSubgraph, twintwigs, starAdded);


//     while(currentSubgraph != jseq->destPattern) {
//         it = pq.begin();
//         st = std::get<0>(*it);
//         // 要检查有点连接并且没有边连接，由点连接由于堆就是按照交点最多的大根堆排的不用检查
//         if (st.hasCommonEdges(currentSubgraph)) {
//             pq.erase(it);
//             continue;
//         }
//         pq.erase(it);
//         orderedTwintwigs.push_back(st);
//         currentSubgraph.expand(st);
//         if (currentSubgraph == jseq->destPattern) {
//             break;
//         }
//         starAdded[st] = true;
//         updateCommonCount(currentSubgraph, twintwigs, starAdded);
//     }

//     return orderedTwintwigs;
// }

std::vector<TwinTwig>
SplitGraph::greedyStarDecomposition() {
    std::vector<TwinTwig> orderedStars;
    std::vector<TwinTwig> stars;

    std::unordered_set<VertexID> vertexCover;
    std::unordered_set<std::pair<VertexID, VertexID>, PairHash> edgesCovered; // 用于记录已覆盖的边

    // Step 1: 计算顶点覆盖集 (vertex cover)，构造所有星形结构
    for (VertexID u = 0; u < graph->getVerticesCount(); ++u) {
        ui nbr_cnt;
        const ui* nbrs = graph->getVertexNeighbors(u, nbr_cnt);
        for (ui i = 0; i < nbr_cnt; ++i) {
            VertexID v = nbrs[i];

            // 如果这条边还没有被覆盖，选择一个端点加入顶点覆盖集
            if (edgesCovered.find({u, v}) == edgesCovered.end() && edgesCovered.find({v, u}) == edgesCovered.end()) {

                /*
                    1. 以u为中心构造star
                    2. 所有uv/vu加入edgesCovered
                */
                std::vector<ui> vs;
                std::vector<std::pair<ui, ui>> es;
                vs.push_back(u);
                for (ui j = 0; j < nbr_cnt; ++j) {
                    vs.push_back(nbrs[j]);
                    es.push_back({u, nbrs[j]});
                    // es.push_back({nbrs[j], u});
                }
                stars.push_back(TwinTwig(vs, es));

                edgesCovered.insert({u, v}); // 标记该边为已覆盖
                edgesCovered.insert({v, u}); // 因为是无向图，两个方向的边都要标记
                break;
            }
        }
    }

    /*
        Step 2: 按照交集要求调整星形结构的顺序
        点交集最大的 & 边交集最少 & 本身边最多的，越靠前
        大根堆
    */
    TwinTwig currentSubgraph;
    
    std::unordered_map<TwinTwig, bool> starAdded;
    // 初始化starpq，把所有p_i都压进去
    // LOG() << "stars.size == " << stars.size() << std::endl;
    // LOG() << "constructed stars : " << std::endl;
    // for (auto& star : stars) {
    //     star.print();
    // }
    // LOG() << "stars print over." << std::endl;
    for (auto& star : stars) {
        starpq.insert(std::make_tuple(star, 0, 0));
        starAdded[star] = false;
    }

    // LOG() << "starpq.size == " << starpq.size() << std::endl;
    // LOG() << "constructed starpq : " << std::endl;
    // for (auto it = starpq.begin(); it != starpq.end(); ++it) {
    //     auto& [star, commonVertexCount, commonEdgeCount] = *it;
    //     star.print();
    // }
    // LOG() << "starpq print over." << std::endl;

    auto it = starpq.begin();
    TwinTwig st = std::get<0>(*it); // 假设需要的是std::tuple中的TwinTwig部分
    starpq.erase(it);
    // LOG() << "after st starpq.size == " << starpq.size() << std::endl;
    // LOG() << "poped starpq : " << std::endl;
    // for (auto it = starpq.begin(); it != starpq.end(); ++it) {
    //     auto& [star, commonVertexCount, commonEdgeCount] = *it;
    //     star.print();
    // }

    orderedStars.push_back(st);
    currentSubgraph = st;
    // LOG() << "currentSubgraph : " << std::endl;
    // currentSubgraph.print();
    starAdded[st] = true;
    updateCommonCount(currentSubgraph, stars, starAdded);

    // LOG() << "print jseq->destPattern: " << std::endl;
    // jseq->destPattern.print();

    while(currentSubgraph != jseq->destPattern) {
        // LOG() << "print currentSubgraph: " << std::endl;
        // currentSubgraph.print();
        if (checkOverTime()) {
            return orderedStars;
        }
        // 按照最小值，找到第一个有共同点的star
        bool found = false;
        // LOG() << "before pop starpq.size == " << starpq.size() << std::endl;
        for (auto it = starpq.begin(); it != starpq.end(); ++it) {
            auto& [star, commonVertexCount, commonEdgeCount] = *it;
            if (currentSubgraph.hasCommonVertices(star)) {
                if (!star.isIncluded(currentSubgraph)) {
                    st = star;
                    starpq.erase(it); // `erase` 返回下一个有效迭代器
                    // LOG() << "erase starpq.size == " << starpq.size() << std::endl;
                    found = true;
                    break;             // 删除后退出循环
                } else {
                    // LOG() << "this star not ok : " << std::endl;
                    // star.print();
                }
            }
        }
        if (!found) {
            LOG() << "not found new star" << std::endl;
            LOG() << "starpq.size == " << starpq.size() << std::endl;
            for (auto it = starpq.begin(); it != starpq.end(); ++it) {
                auto& [star, commonVertexCount, commonEdgeCount] = *it;
                LOG() << "star : " << std::endl;
                star.print();
            }
            abort();
        }
        // LOG() << "st : " << std::endl;
        // st.print();
        // LOG() << "after pop starpq.size == " << starpq.size() << std::endl;
        orderedStars.push_back(st);
        currentSubgraph.expand(st);
        // LOG() << "currentSubgraph : " << std::endl;
        // currentSubgraph.print();
        if (currentSubgraph == jseq->destPattern) {
            break;
        }
        starAdded[st] = true;
        updateCommonCount(currentSubgraph, stars, starAdded);
    }

    return orderedStars;
}

std::vector<TwinTwig>
SplitGraph::optimalDecomposition() {
    std::vector<TwinTwig> backwards;
    auto start = std::chrono::steady_clock::now();
    ui qm = graph->getEdgesCount();
    ui qn = graph->getVerticesCount();
    ui M = d_graph->getVerticesCount();
    auto& deltaCosts = jseq->deltaCosts;
    std::unordered_map<TwinTwig, ui> validTwinTwig;

    for (ui m = 1; m < graph->getEdgesCount(); ++m) {
        for (ui n = 2; n < graph->getVerticesCount(); ++n) {
            if (checkOverTime()) {
                return backwards;
            }
            deltaCosts[std::tuple(m, n, 0, 0)] = 0;
            for (ui delta_m = 1; delta_m <= graph->getEdgesCount() - m; ++delta_m) {
                for (ui delta_n = 2; delta_n < graph->getVerticesCount(); ++delta_n) {
                    // 要找最小值
                    deltaCosts[std::tuple(m, n, delta_m, delta_n)] = std::numeric_limits<double>::max();
                    for (ui a = 1; a <= 2; ++a) {
                        for (ui b = 0; b <= a; ++b) {
                            // 要确保 a <= delta_m && b <= delta_n
                            if (a > delta_m || b > delta_n) {
                                continue;
                            }
                            double cost = deltaCosts[std::tuple(m, n, delta_m - a, delta_n - b)]
                                        + 3 * card(m + delta_m, n + delta_n) + card(a, b) + M;
                            deltaCosts[std::tuple(m, n, delta_m, delta_n)] = std::min(deltaCosts[std::tuple(m, n, delta_m, delta_n)], cost);
                        }
                    }
                }
            }
        }
    }

    // 1. 构造所有twintwig
    std::unordered_set<TwinTwig> twinTwigs;
    // std::vector<std::tuple<TwinTwig, ui, ui>> twinTwigs;
    for (ui u = 0; u < graph->getVerticesCount(); ++u) {
        ui nbr_cnt = 0;
        const ui* nbrs = graph->getVertexNeighbors(u, nbr_cnt);
        for (ui j = 0; j < nbr_cnt; ++j) {
            VertexID v = nbrs[j];
            // 分别是 costValue 和 traceCost
            ui traceCost = card(1, 2);
            ui deltaCost = deltaCosts[std::tuple(1, 2, qm, qn)];
            ui costValue = traceCost + deltaCost;
            twinTwigs.insert(TwinTwig({u, v}, {{u, v}}));
            // twinTwigs.push_back(std::make_tuple(TwinTwig({u, v}, {{u, v}}), costValue, traceCost));
            std::vector<TwinTwig> backwards;
            backwards.push_back(TwinTwig({u, v}, {{u, v}}));
            twintwigpq.insert(std::make_tuple(TwinTwig({u, v}, {{u, v}}), costValue, traceCost, backwards));
            validTwinTwig[TwinTwig({u, v}, {{u, v}})] = costValue;
        }
    }
    for (ui u = 0; u < graph->getVerticesCount(); ++u) {
        ui nbr_cnt = 0;
        const ui* nbrs = graph->getVertexNeighbors(u, nbr_cnt);
        for (ui j = 0; j < nbr_cnt; ++j) {
            for (ui k = j + 1; k < nbr_cnt; ++k) {
                VertexID v = nbrs[j];
                VertexID w = nbrs[k];
                ui traceCost = card(2, 3);
                ui deltaCost = deltaCosts[std::tuple(2, 3, qm, qn)];
                ui costValue = traceCost + deltaCost;
                twinTwigs.insert(TwinTwig({u, v, w}, {{u, v}, {u, w}}));
                // twinTwigs.push_back(std::make_tuple(TwinTwig({u, v, w}, {{u, v}, {u, w}}), costValue, traceCost));
                std::vector<TwinTwig> backwards;
                backwards.push_back(TwinTwig({u, v, w}, {{u, v}, {u, w}}));
                twintwigpq.insert(std::make_tuple(TwinTwig({u, v, w}, {{u, v}, {u, w}}), costValue, traceCost, backwards));
                validTwinTwig[TwinTwig({u, v, w}, {{u, v}, {u, w}})] = costValue;
            }
        }
    }

    /*
        f(n) = g(n) + h(n)
        Costs: Cost(D_i, P) = f(n)
        deltaCosts: Cost(D_i) = g(n)
        deltaCosts: Delta_Cost(D_i, P) = h(n)
    */
    /*
        初始化 deltaCosts
        不同的Pi({m, n}对)之间是没有递推关系的
    */

    auto end = std::chrono::steady_clock::now();
    double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // LOG() << "initAllCosts_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;

    // for (auto& [twinTwig, costValue, traceCost] :twinTwigs) {
    //     traceCost = card(twinTwig.getEdgesCount(), twinTwig.getVerticesCount());
    //     deltaCost = deltaCosts[std::tuple(twinTwig.getEdgesCount(), twinTwig.getVerticesCount(), qm, qn)];
    //     costValue  = traceCost + 
    //     state.costValue = state.traceCost + state.deltaCost;
    // }

    // LOG() << "twinTwigs.size == " << twinTwigs.size() << std::endl;

    TwinTwig currentSubgraph;
    
    ui currentCostValue;
    ui currentTraceCost;
    

    auto it = twintwigpq.begin();
    std::tie(currentSubgraph, currentCostValue, currentTraceCost, backwards) = *it;
    twintwigpq.erase(it);
    // LOG() << "currentSubgraph : , backwards.size == " << backwards.size() << std::endl;
    // currentSubgraph.print();
    // for (auto& tw : backwards) {
    //     tw.print();
    // }

    while(currentSubgraph != jseq->destPattern) {
        if (checkOverTime()) {
            return backwards;
        }
        for (auto& twintwig : twinTwigs) {
            if (currentSubgraph.hasCommonVertices(twintwig) && !currentSubgraph.hasCommonEdges(twintwig) && !twintwig.isIncluded(currentSubgraph)) {
                auto tmpbackwards = backwards;
                TwinTwig newSubgraph = currentSubgraph;
                newSubgraph.expand(twintwig);
                ui newTraceCost = currentTraceCost
                            + 3 * card(newSubgraph.getEdgesCount(), newSubgraph.getVerticesCount())
                            + card(twintwig.getEdgesCount(), twintwig.getVerticesCount()) + M;
                ui dm = graph->getEdgesCount() - newSubgraph.getEdgesCount();
                ui dn = graph->getVerticesCount() - newSubgraph.getVerticesCount();
                ui newDeltaCost = deltaCosts[std::tuple(newSubgraph.getEdgesCount(), newSubgraph.getVerticesCount(), dm, dn)];
                ui newCostValue = newTraceCost + newDeltaCost;
                // LOG() << "before expand : , backwards.size == " << backwards.size() << std::endl;
                // currentSubgraph.print();
                // for (auto& tw : tmpbackwards) {
                //     tw.print();
                // }
                // LOG() << "now twintwig : " << std::endl;
                // twintwig.print();
                tmpbackwards.push_back(twintwig);
                // LOG() << "newSubgraph : , backwards.size == " << tmpbackwards.size() << std::endl;
                // newSubgraph.print();
                // for (auto& tw : tmpbackwards) {
                //     tw.print();
                // }
                auto newTuple = std::make_tuple(newSubgraph, newCostValue, newTraceCost, tmpbackwards);
                /*
                    逻辑：
                    1. validTwinTwig里面存储了这个twintwig对应的最小的costValue
                    2. 如果find==end，说明这个twintwig第一次加入pq，直接insert
                    3. 如果find!=end，说明这个twintwig已经加入过pq
                        (1) 这次的costValue更小，需要更新validTwinTwig，并且插入（这里后面在pq pop的时候要判定，如果和min costValue一致，才是有效的）
                        (2) 这次更大，就不用做insert操作

                 */
                if (validTwinTwig.find(newSubgraph) == validTwinTwig.end()) {
                    validTwinTwig[newSubgraph] = newCostValue;
                    twintwigpq.insert(newTuple);
                } else {
                    if (newCostValue < validTwinTwig[newSubgraph]) {
                        validTwinTwig[newSubgraph] = newCostValue;
                        twintwigpq.insert(newTuple);
                    } else {
                        // do nothing
                    }
                }
            }
        }
        // LOG() << "after update pq, pq.size == " << twintwigpq.size() << std::endl;
        // 每次pop都要找那个valid的tuple
        it = twintwigpq.begin();
        std::tie(currentSubgraph, currentCostValue, currentTraceCost, backwards) = *it;
        while (validTwinTwig[currentSubgraph] != currentCostValue) {
            assert(currentCostValue > validTwinTwig[currentSubgraph]);
            if (currentCostValue < validTwinTwig[currentSubgraph]) {
                LOG() << "currentSubgraph : " << std::endl;
                currentSubgraph.print();
                LOG() << "validTwinTwig[currentSubgraph] == " << validTwinTwig[currentSubgraph] << ", currentCostValue == " << currentCostValue << std::endl;
                LOG() << "currentCostValue == " << currentCostValue << std::endl;
                LOG() << "Error: should be bigger than validTwinTwig" << std::endl;
                abort();
            }
            twintwigpq.erase(it);
            // LOG() << "erase not valid tuple" << std::endl;
            it = twintwigpq.begin();
            std::tie(currentSubgraph, currentCostValue, currentTraceCost, backwards) = *it;
        }
        twintwigpq.erase(it);
        // LOG() << "currentSubgraph : , backwards.size == " << backwards.size() << std::endl;
        // currentSubgraph.print();
        // for (auto& tw : backwards) {
        //     tw.print();
        // }
    }

    end = std::chrono::steady_clock::now();
    single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // LOG() << "optimalDecomposition_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;
    // LOG() << "output backwards.size == " << backwards.size() << std::endl;
    // for (auto& tw : backwards) {
    //     tw.print();
    // }
    return backwards;
}

void
SplitGraph::split_twintwig() {
    jseq = new JoinSequence(graph->getVerticesCount());
    jseq->destPattern = getDestPattern();
    // LOG() << "print destPattern : " << std::endl;
    // jseq->destPattern.print();

    std::vector<TwinTwig> patterns = optimalDecomposition();
    if (checkOverTime()) {
        return;
    }
    // std::vector<TwinTwig> patterns = greedyTwinTwigDecomposition();
    // LOG() << "sub twintwig count == " << patterns.size() << std::endl;
    // LOG() << "Over optimal decompose : " << std::endl;
    // for (ui i = 0; i < patterns.size(); ++i) {
    //     patterns[i].print();
    // }
    saveSubgraph(patterns);
}

void
SplitGraph::split_star() {
    jseq = new JoinSequence(graph->getVerticesCount());
    jseq->destPattern = getDestPattern();
    // LOG() << "print destPattern : " << std::endl;
    // jseq->destPattern.print();

    std::vector<TwinTwig> patterns = greedyStarDecomposition();
    if (checkOverTime()) {
        return;
    }
    // LOG() << "sub star count == " << patterns.size() << std::endl;
    // for (ui i = 0; i < patterns.size(); ++i) {
    //     patterns[i].print();
    // }
    saveSubgraph(patterns);
}

void
SplitGraph::split_seed() {
    jseq = new JoinSequence(graph->getVerticesCount());
    jseq->destPattern = getDestPattern();
    // LOG() << "print destPattern : " << std::endl;
    // jseq->destPattern.print();

    std::vector<TwinTwig> patterns = optimalDecomposition();
    if (checkOverTime()) {
        return;
    }
    // LOG() << "sub twintwig size == " << patterns.size() << std::endl;
    // std::vector<TwinTwig> patterns = greedyTwinTwigDecomposition();

    // add third edge
    for (ui ii = 0; ii < patterns.size(); ++ii) {
        auto& p = patterns[ii];
        if (p.getEdgesCount() == 2) {
            // LOG() << "process a double edged twintwig" << std::endl;
            std::vector<ui> vs;
            std::vector<std::pair<ui, ui>> es;
            p.getVerticesList(vs);
            p.getEdgesList(es);
            ui root;
            ui us[2];
            ui cnt = 0;
            assert(vs.size() == 3);
            assert(es.size() == 2);
            for (ui i = 0; i < vs.size(); ++i) {
                if (p.getVertexNbrCount(vs[i]) == 2) {
                    root = vs[i];
                } else if (p.getVertexNbrCount(vs[i]) == 1) {
                    us[cnt++] = vs[i];
                }
            }
            assert(cnt == 2);
            bool flag = true;
            for (ui i = 0; i < vs.size(); ++i) {
                if (root > vs[i]) {
                    flag = false;
                }
            }
            // try to add edge if exists in original query graph
            if (flag) {
                // check if the edge exists in original query graph
                ui nbr_cnt;
                const ui* nbrs = graph->getVertexNeighbors(us[0], nbr_cnt);
                for (ui i = 0; i < nbr_cnt; ++i) {
                    if (nbrs[i] == us[1]) {
                        es.push_back({us[0], us[1]});
                        break;
                    }
                }

                // add a new twintwig
                // LOG() << "add a new edge : " << us[0] << " -> " << us[1] << "to twintwig :" << std::endl;
                // p.print();
                patterns[ii] = TwinTwig(vs, es);
            } else {
                // LOG() << "can not add a new edge : " << us[0] << " -> " << us[1] << "to twintwig :" << std::endl;
            }
        }
    }
    // LOG() << "sub seed count == " << patterns.size() << std::endl;
    // for (ui i = 0; i < patterns.size(); ++i) {
    //     patterns[i].print();
    // }
    saveSubgraph(patterns);
}

void
SplitGraph::updateVisitedNbrCnt(std::vector<ui>& vec, ui u) {
    ui neighbor_count = 0;
    const ui* neighbors = graph->getVertexNeighbors(u, neighbor_count);
    for (ui i = 0; i < neighbor_count; ++i) {
        ui neighbor = neighbors[i];
        vec[neighbor]++;
    }
}

void
SplitGraph::saveSubgraph(std::vector<TwinTwig>& tws) {
    // LOG() << "total graph's size  = " << graph->getVerticesCount() << std::endl;
    for (ui i = 0; i < tws.size(); i++) {
        subgraphs.push_back(new InducedSubgraph(graph->getVerticesCount(), graph->getLabelsCount())); 
        Graph* subgraph = subgraphs[subgraph_count]->subgraph;
        ui* old2new = subgraphs[subgraph_count]->old2news;
        ui* new2old = subgraphs[subgraph_count]->new2olds;
        ui* l_old2new = subgraphs[subgraph_count]->l_old2news;
        ui* l_new2old = subgraphs[subgraph_count]->l_new2olds;
        std::vector<ui> path;
        tws[i].getVerticesList(path);
        ui* path_data = subgraphs[subgraph_count]->path;

        // LOG() << "printTwinTwig " << subgraph_count << " : " << std::endl;
        // tws[i].print();

        // NOTE: 这里不能是诱导子图了，而是要指定(vertexList, EdgeList)
        std::vector<std::pair<ui, ui>> edges;
        tws[i].getBidirectEdgesList(edges);
        // LOG() << "printEdges " << "size == " << edges.size() << " : " << std::endl;
        // for (ui i = 0; i < edges.size(); ++i) {
        //     LOG() << edges[i].first << " -> " << edges[i].second << std::endl;
        // }
        // LOG() << "inducedSubgraph's total_vertices_count == " << subgraphs[subgraph_count]->total_vertices_count << std::endl;
        subgraph->loadsubgFromGraph(graph, tws[subgraph_count].vertices_, edges, old2new, new2old, l_old2new, l_new2old);
        // LOG() << "print subgraph : " << std::endl;
        // subgraph->printGraph();
        std::copy(path.begin(), path.end(), path_data);
        subgraph_count++;
    }
}

void
SplitGraph::saveSubgraph(std::vector<ui>& path) {
    // LOG() << "total graph's size  = " << graph->getVerticesCount() << std::endl;
    subgraphs.push_back(new InducedSubgraph(graph->getVerticesCount(), graph->getLabelsCount()));

    Graph* subgraph = subgraphs[subgraph_count]->subgraph;
    ui* old2new = subgraphs[subgraph_count]->old2news;
    ui* new2old = subgraphs[subgraph_count]->new2olds;
    ui* l_old2new = subgraphs[subgraph_count]->l_old2news;
    ui* l_new2old = subgraphs[subgraph_count]->l_new2olds;

    ui* path_data = subgraphs[subgraph_count]->path;

    subgraph->loadsubgFromGraph(graph, path, old2new, new2old, l_old2new, l_new2old);
    std::copy(path.begin(), path.end(), path_data);

    // std::ostringstream oss;
    // oss << "printPath" << subgraph_count << " : " << std::endl;
    // for (size_t i = 0; i < path.size() - 1; i++) {
    //     oss << path[i] << " -> " << path[i+1] << std::endl;
    // }
    // oss << std::endl;
    // LOG() << oss.str();
}

void 
SplitGraph::printPaths() {
    for (int i = 0; i < subgraph_count; ++i) {
        InducedSubgraph* inducedSubgraph = subgraphs[i];
        ui vcnt = inducedSubgraph->subgraph->getVerticesCount();
        ui* path = inducedSubgraph->path;
        std::ostringstream oss;
        oss << "printPath" << i << " : " << std::endl;
        for (size_t j = 0; j < vcnt - 1; j++) {
            oss << path[j] << " -> " << path[j+1] << std::endl;
        }
        oss << std::endl;
        LOG() << oss.str();
    }
}

std::vector<InducedSubgraph*>
SplitGraph::getSubgraphs() {
    return subgraphs;
}

int
SplitGraph::getSubgraphCount() {
    return subgraph_count;
}


void SplitGraph::addBranchNode() {
    if (stk->empty()) {
        // 说明是root的回溯了，不用再往上
        return;
    }
    ui u = stk->top();
    if (jtree->branchNodes.find(u) != jtree->branchNodes.end()) {
        return;
    }
    ui u_nbrs_count;
    const ui* u_nbrs = graph->getVertexNeighbors(u, u_nbrs_count);

    // std::ostringstream oss;
    // oss << "u " << u << "'s nbrs" << std::endl;
    // for (ui i = 0; i < u_nbrs_count; ++i) {
    //     oss << u_nbrs[i] << "-" << visited[u_nbrs[i]] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // 统计未访问的邻居数量
    ui unvisited_count = 0;
    for (ui i = 0; i < u_nbrs_count; ++i) {
        if (!visited[u_nbrs[i]]) {
            unvisited_count++;
            break;
        }
    }

    // LOG() << "u = " << u << ", unvisited_count = " << unvisited_count << std::endl;

    // 检查是否为分叉节点
    if (unvisited_count == 0) {
        return;
    }
    /*
        u是一个分叉节点
        (1) 获取点u对应的BranchNode
        (2) 加入 uid 信息和 default parent信息

     */
    jtree->setBranchNode(u);
}
