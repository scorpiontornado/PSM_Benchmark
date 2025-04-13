#ifndef _JOINCOLLECT_H
#define _JOINCOLLECT_H

#include "JoinCollect.h"

JoinCollect::JoinCollect(Graph* data_graph, Graph* g, JoinTree* jtree, ui cnt, int64_t time_limit, TaskPool* taskPool)
    : data_graph(data_graph), graph(g), jtree(jtree), unit_cnt(cnt), time_limit(time_limit), taskPool(taskPool) {
    partial_matches = new PartialMatch*[unit_cnt];
    visited = new bool[data_graph->getVerticesCount()]();
    visited_flag = new ui[data_graph->getVerticesCount()]();
    // LOG() << "JoinCollect time_limit == " << time_limit << std::endl;
}

JoinCollect::JoinCollect(Graph* data_graph, Graph* g, JoinSequence* jseq, ui cnt, int64_t time_limit, TaskPool* taskPool)
    : data_graph(data_graph), graph(g), jseq(jseq), unit_cnt(cnt), time_limit(time_limit), taskPool(taskPool) {
    partial_matches = new PartialMatch*[unit_cnt];
    visited = new bool[data_graph->getVerticesCount()]();
    visited_flag = new ui[data_graph->getVerticesCount()]();
    // LOG() << "JoinCollect time_limit == " << time_limit << std::endl;
}

JoinCollect::~JoinCollect() {
    delete[] partial_matches;
    delete[] visited;
    delete[] visited_flag;
}

void
JoinCollect::getRowKey(ui begin_idx, std::vector<ui>& matches, std::set<ui>& joinKeys, std::vector<ui>& key) {
    // 确保 matches 的索引合法
    ui cnt = 0;
    for (auto& k : joinKeys) {
        key[cnt++] = matches[k + begin_idx];
    }

    // std::ostringstream oss;

    // oss << "joinKeys : ";
    // for (auto& k: joinKeys) {
    //     oss << k << ", ";
    // }
    // oss << std::endl;

    // oss << "match : ";
    // for (ui i = begin_idx; i < begin_idx + joinKeys.size(); i++) {
    //     oss << matches[i] << ", ";
    // }
    // oss << std::endl;

    // oss << "got key : ";
    // for (ui i = 0; i < key.size(); i++) {
    //     oss << key[i] << ", ";
    // }
    // oss << std::endl;

    // LOG() << oss.str();
}

// 把这个函数改成并行的就行了
PartialMatch* 
JoinCollect::concateTwoTabels(PartialMatch* R, PartialMatch* S) {
    /*
        1. 构造不同类型的joinKey
        2. 进行join，join全部封装在utility里面
     */
    

    // auto start = std::chrono::steady_clock::now();

    // 1. 获取joinKeys
    // std::ostringstream oss;
    // LOG() << "hashjoin function" << std::endl;
    std::unordered_map<ui, int> allUidSet;
    // LOG() << "R length == " << R->length << ", S length == " << S->length << std::endl;
    // oss << "R's old uids : ";
    for (ui i = 0; i < R->length; ++i) {
        // oss << R->new2olds[i] << ", ";
        if (allUidSet.find(R->new2olds[i]) == allUidSet.end()) {
            allUidSet[R->new2olds[i]] = 1;
        } else {
            allUidSet[R->new2olds[i]]++;
        }    
    }
    // oss << std::endl;
    // oss << "S's old uids : ";
    for (ui i = 0; i < S->length; ++i) {
        // oss << S->new2olds[i] << ", ";
        if (allUidSet.find(S->new2olds[i]) == allUidSet.end()) {
            allUidSet[S->new2olds[i]] = 1;
        } else {
            allUidSet[S->new2olds[i]]++;
        }
    }
    // oss << std::endl;
    // LOG() << oss.str();

    std::unordered_map<ui, int> bothUidSet;
    for (auto& it : allUidSet) {
        if (it.second == 2) {
            bothUidSet[it.first] = 2;
        }
    }
    // for (auto it = bothUidSet.begin(); it != bothUidSet.end(); ++it) {
    //     LOG() << "hashjoin old uid == " << it->first << ", cnt == " << it->second << std::endl;
    // }
    // 这两个存的是newid，newid和idx是一样的, 0~length-1
    std::unordered_set<ui> RjoinIdxs;
    std::unordered_set<ui> SjoinIdxs;
    for (auto& it : bothUidSet) {
        if (it.second == 2) {
            // LOG() << "join key (old uid) == " << it.first << std::endl;
            // LOG() << "Rnewid == " << R->old2news[it.first] << ", Snewid == " << S->old2news[it.first] << std::endl;
            RjoinIdxs.insert(R->old2news[it.first]);
            SjoinIdxs.insert(S->old2news[it.first]);
        }
    }

    ui new_length = R->length + S->length - bothUidSet.size();

    NoPartitionHashJoin* npjoin = new NoPartitionHashJoin(data_graph->getVerticesCount(), R, S, RjoinIdxs, SjoinIdxs, new_length,
        TaskSlot::g_args->time_limit, data_graph->getVerticesCount(), visited, visited_flag, taskPool->THREADS_NUMBER, joinround);

    // auto end = std::chrono::steady_clock::now();
    // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // LOG() << "prehashjoin_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;

    PartialMatch* res;
    if (taskPool->THREADS_NUMBER > 1) {
        // res = npjoin->execute_parallel();
        res = npjoin->execute();
    } else {
        res = npjoin->execute();
    }

    /*
        double find_key_time{0.0};
        double construct_time{0.0};
        double extend_time{0.0};
        double copy_S_time{0.0};
        double copy_R_time{0.0};
        double copy_visited_time{0.0};
    */
    // LOG() << "find_key_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->find_key_time) << " s" << std::endl;
    // LOG() << "total_construct_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->total_construct_time) << " s" << std::endl;
    // LOG() << "construct_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->construct_time) << " s" << std::endl;
    // LOG() << "exact_construct_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->exact_construct_time) << " s" << std::endl;
    // LOG() << "extend_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->extend_time) << " s" << std::endl;
    // LOG() << "copy_S_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->copy_S_time) << " s" << std::endl;
    // LOG() << "copy_R_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->copy_R_time) << " s" << std::endl;
    // LOG() << "copy_visited_time == " << intTimer::TEMPNANOSECTOSEC(npjoin->copy_visited_time) << " s" << std::endl;

    return res;
}

PartialMatch*
JoinCollect::recur_collect(ui u) {
    if (jtree->leafNodes.find(u) != jtree->leafNodes.end()) {
        // 把u这个叶子对应的subgraph_id返回回去
        // LOG() << "leaf u == " << u << ", subgraph_id == " << jtree->leafNodes[u].subgraph_id
        //       << ", match.size == " << partial_matches[jtree->leafNodes[u].subgraph_id]->size
        //       << ", length == " << partial_matches[jtree->leafNodes[u].subgraph_id]->length << std::endl;
        // LOG() << "finished join node == " << u << std::endl;
        return partial_matches[jtree->leafNodes[u].subgraph_id];
    }

    PartialMatch* result;

    bool isFirst = true; // 对于每一个branchNode用于初始化第一个表
    // LOG() << "branch u == " << u << std::endl;
    // LOG() << "join node == " << u << std::endl;
    for (ui i = 0; i < jtree->childs[u].size(); i++) {
        // LOG() << "join " << i << "-th child of node " << u << std::endl;
        ui chd = jtree->childs[u][i];
        PartialMatch* childResult = recur_collect(chd); // 递归获取子节点结果
        if (checkOverTime()) {
            // LOG() << "recur_collect overtime, exit" << std::endl;
            if (joinround == unit_count) {
                return result;   
            }
            // LOG() << "return nullptr" << std::endl;
            return handleResp();
        }

        // LOG() << "child u == " << chd << ", childResult size == " << childResult->size << std::endl;
        if (isFirst) {
            result = childResult;
            isFirst = false;
        } else {
            // LOG() << "start hashjoin Rchild == " << chd << std::endl;
            // LOG() << "left size == " << result->size << ", right size == " << childResult->size << std::endl;
            // auto start = std::chrono::steady_clock::now();
            result = concateTwoTabels(result, childResult); // 递归 join
            // auto end = std::chrono::steady_clock::now();
            // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // std::ostringstream oss;
            // oss << "joinround: " << joinround++ << ", ";
            // oss << "single_round_hashjoin_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns);
            // oss << ", single_round_result.size == " << result->size << std::endl;
            // LOG() << oss.str();

            if (checkOverTime()) {
                // LOG() << "recur_collect overtime, exit" << std::endl;
                if (joinround == unit_count) {
                    return result;   
                }
                // LOG() << "return nullptr" << std::endl;
                return handleResp();
            }

            delete childResult;
        }
        if (checkOverTime()) {
            // LOG() << "recur_collect overtime, exit" << std::endl;
            if (joinround == unit_count) {
                return result;   
            }
            return handleResp();
        }
        // std::ostringstream oss;
        // oss << "after join childs [";
        // for (ui j = 0; j <= i; j++) {
        //     oss << jtree->childs[u][j] << ", ";
        // }
        // oss << "], result size == " << result->size << std::endl;
        // LOG() << oss.str();

        // result->logTop10();
    }
    // LOG() << "finished join node == " << u << std::endl;
    if (u == jtree->jointree_root) {
        // LOG() << "log all result : " << std::endl;
        // result->logAll2();
    }
    return result;
}

PartialMatch*
JoinCollect::sequence_collect() {
    // 这里直接subgraph_id从0开始，然后取对应partial_match就可以了
    // LOG() << partial_matches[0]->size << std::endl;
    PartialMatch* result;

    for (ui subgraph_id = 0; subgraph_id < unit_cnt; ++subgraph_id) {
        // LOG() << "subgraph_id == " << subgraph_id
        //     << ", partialMatch size == " << partial_matches[subgraph_id]->size << std::endl;
        // partial_matches[subgraph_id]->logAll();
        if (subgraph_id == 0) {  
            // LOG() << "subgraph_id == " << subgraph_id << std::endl;

            std::unordered_map<ui, int> allUidSet;
            PartialMatch* R = partial_matches[subgraph_id];
            // LOG() << "R length == " << R->length << std::endl;
            // std::ostringstream oss;
            // oss << "R's old uids : ";
            for (ui i = 0; i < R->length; ++i) {
                // oss << R->new2olds[i] << ", ";
                if (allUidSet.find(R->new2olds[i]) == allUidSet.end()) {
                    allUidSet[R->new2olds[i]] = 1;
                } else {
                    allUidSet[R->new2olds[i]]++;
                }    
            }
            // oss << std::endl;
            // LOG() << oss.str();
            // auto start = std::chrono::steady_clock::now();

            result = partial_matches[subgraph_id];

            // auto end = std::chrono::steady_clock::now();
            // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // LOG() << "single_hashjoin_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;
            if (result) {
            } else {
                // LOG() << "result == nullptr" << std::endl;
                return handleResp();
            }
        } else {
            // LOG() << "subgraph_id == " << subgraph_id << std::endl;
            // LOG() << "left.length == " << result->length << ", right.length == " << partial_matches[subgraph_id]->length << std::endl;
            // LOG() << "left.size == " << result->size << ", right.size == " << partial_matches[subgraph_id]->size << std::endl;
            PartialMatch* oldresult = result;
            // auto start = std::chrono::steady_clock::now();
            result = concateTwoTabels(result, partial_matches[subgraph_id]);
            // auto end = std::chrono::steady_clock::now();
            // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // std::ostringstream oss;
            // oss << "joinround: " << joinround++ << ", ";
            // oss << "single_round_hashjoin_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns);
            // oss << ", single_round_result.size == " << result->size << std::endl;
            // LOG() << oss.str();
            if (result) {
            } else {
                // LOG() << "result == nullptr" << std::endl;
                return handleResp();
            }
            if (checkOverTime()) {
                // LOG() << "sequence_collect overtime, exit" << std::endl;
                if (joinround == unit_count) {
                    return result;   
                }
                return handleResp();
            }
            delete oldresult;
            oldresult = nullptr;
            delete partial_matches[subgraph_id];
            partial_matches[subgraph_id] = nullptr;
        }
        // LOG() << "result.size == " << result->size << std::endl;
        // LOG() << "-------logTop10 : " << std::endl;
        // result->logTop10();
    }
    // LOG() << "log all result : " << std::endl;
    // result->logAll2();
    return result;
}

bool
JoinCollect::checkOverTime() {
    if (intTimer::getClockNano() >= time_limit) {
        overtime = true;
        return true;
    }
    return false;
}

PartialMatch*
JoinCollect::handleResp() {
    return nullptr;
}

#endif // _JOINCOLLECT_H
