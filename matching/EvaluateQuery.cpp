#include "EvaluateQuery.h"
#include "utility/computesetintersection.h"
#include "utility/pretty_print.h"
#include <vector>
#include <cstring>

#if ENABLE_QFLITER == 1
BSRGraph ***EvaluateQuery::qfliter_bsr_graph_;
int *EvaluateQuery::temp_bsr_base1_ = nullptr;
int *EvaluateQuery::temp_bsr_state1_ = nullptr;
int *EvaluateQuery::temp_bsr_base2_ = nullptr;
int *EvaluateQuery::temp_bsr_state2_ = nullptr;
#endif

#ifdef SPECTRUM
bool EvaluateQuery::exit_;
#endif

#ifdef DISTRIBUTION
size_t* EvaluateQuery::distribution_count_;
#endif

std::function<void(TaskSlot&, int)> EvaluateQuery::enum_method = nullptr;


size_t
EvaluateQuery::ParallelExecute(TaskPool* taskPool,  SplitGraph* splitGraph, UnitArgs** unitArgsVec) {
    auto startPara = std::chrono::steady_clock::now();
    // printCandidates(TaskSlot::g_args->candidates, TaskSlot::g_args->candidates_count,
    //     TaskSlot::g_args->query_graph->getVerticesCount(),
    //     TaskSlot::g_args->data_graph->getGraphMaxLabelFrequency());
    // printEdgeMatrix(TaskSlot::g_args->edge_matrix, TaskSlot::g_args->query_graph->getVerticesCount());

    // initially create THREADS_COUNT tasks
    taskPool->initTasks();

    // startPara
    auto endinit = std::chrono::steady_clock::now();
    int64_t enumeration_time_in_ns_init = std::chrono::duration_cast<std::chrono::nanoseconds>(endinit - startPara).count();
    LOG() << "InitTasks time: " << NANOSECTOSEC(enumeration_time_in_ns_init) << std::endl;

    if (TaskSlot::g_args->overtime.load()) {
        return 0;
    }

    // thread_num == 1
    // if (taskPool->THREADS_NUMBER == 1) {
    if (TaskSlot::g_args->splitMode == SPLITMODE::NOSPLIT) {
        // LOG() << "Single thread mode." << std::endl;
        SingleExecute(taskPool);
        return 0;
    }

    // LOG() << "splitMode = " << (TaskSlot::g_args->splitMode == SPLITMODE::Q ? "Q" : "C") << std::endl;
    if (TaskSlot::g_args->splitMode == SPLITMODE::Q) {
        // split_Q
        // LOG() << "execute id_unit == " << 0 << std::endl;
        std::vector<std::thread> threads;
        for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
            threads.emplace_back(SingleExecute, taskPool);
        }
        for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
            threads[i].join();
        }
        if (TaskSlot::g_args->overtime.load()) {
            // LOG() << "Query Time out." << std::endl;
            return 0;
        }

        for (int id_unit = 1; id_unit < TaskSlot::g_args->count_unit; id_unit++) {
            // LOG() << "execute id_unit == " << id_unit << std::endl;
            taskPool->candidateInitTasks(id_unit);
            std::vector<std::thread> threads;
            for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
                threads.emplace_back(SingleExecute, taskPool);
            }
            for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
                threads[i].join();
            }
            if (TaskSlot::g_args->overtime.load()) {
                // LOG() << "Query Time out." << std::endl;
                return 0;
            }
        }

        return 0;
    }

    // thread_num > 1
    // auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
        threads.emplace_back(SingleExecute, taskPool);
    }

    for (uint64_t i = 0; i < taskPool->THREADS_NUMBER; i++) {
        threads[i].join();
    }

    // LOG() << "all threads are over." << std::endl;
    // auto end = std::chrono::steady_clock::now();
    // int64_t enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // // startPara
    // int64_t enumeration_time_in_ns_para = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startPara).count();

    // LOG() << "threads.emplace_back(SingleExecute, taskPool) time == " << NANOSECTOSEC(enumeration_time_in_ns) << std::endl;
    // LOG() << "Parathreads.emplace_back(SingleExecute, taskPool) time == " << NANOSECTOSEC(enumeration_time_in_ns_para) << std::endl;

    return 0;
}

void
EvaluateQuery::handleSingleExecute(std::chrono::steady_clock::time_point enum_start) {
    auto enum_end = std::chrono::steady_clock::now();
    int64_t enumeration_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(enum_end - enum_start).count();
    TaskSlot::g_args->addTimeEnum(std::this_thread::get_id(), enumeration_time_in_ns);
}

void
EvaluateQuery::SingleExecute(TaskPool* taskPool) {
    // LOG() << taskPool->Size() << std::endl;
    auto enum_start = std::chrono::steady_clock::now();

    if (taskPool->THREADS_NUMBER == 1) {
        TaskSlot task;
        int task_id = taskPool->Pop(task);
        // LOG() << "Single Thread Execute Task ID = " << task_id << std::endl;
        if (!task_id) {
            LOG() << "Error: No task available in sinlge thread execution." << std::endl;
            abort();
        }
        if (task_id) {
            // Execute the task, different single enumeration methods
            // LOG() << "Execute task " << task_id << std::endl;
            task.l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
            task.start_task_time = intTimer::getClockNano();
            enum_method(task, task_id);
        }
        // LOG() << "THREADS_NUMBER Finish single thread mode." << std::endl;
        return handleSingleExecute(enum_start);
    }

    while(true) {
        TaskSlot task;
        int task_id = taskPool->Pop(task);
        // LOG() << "Execute Task ID = " << task_id << std::endl;
        if (task_id) {
            // Execute the task, different single enumeration methods
            // LOG() << "Execute task " << task_id << std::endl;
            task.l_args->l_limit_num = TaskSlot::g_args->output_limit_num - TaskSlot::g_args->embedding_count;
            task.start_task_time = intTimer::getClockNano();
            enum_method(task, task_id);
            // LOG() << "Finish task " << task_id << std::endl;
            continue;
        }
        if (TaskSlot::g_args->overtime.load()) {
            // LOG() << "Query Time out." << std::endl;
            taskPool->cv.notify_all();
            return handleSingleExecute(enum_start);
        }
        // LOG() << "No task available." << std::endl;
        // task_id == 0 means that there are no task availabel
        // Idle the thread and ask for splitting a new task

        // Wait for split a new task
        // LOG() << "task.l_args = " << task.l_args << std::endl;
        std::unique_lock<std::mutex> lk(taskPool->queue_mutex);
        taskPool->addIdle();
        if (taskPool->checkOver()) {
            // NOTE: the last idle thread must invoke here, but not wait and be notified
            // 最后一个idle的线程不会走下面的else，先等待再被唤醒再发现结束了
            // 所以其他的线程都会被在下面唤醒，只需要在这里notify_all就可以了
            // LOG() << "first all threads are idle." << std::endl;
            taskPool->cv.notify_all();
            // TODO: handle resp and exit
            return handleSingleExecute(enum_start);
        }
        // LOG() << "wait for task." << std::endl;
        taskPool->cv.wait(lk);
        // LOG() << "Be notified." << std::endl;

        // Received signal, maybe a new task is made, or all threads are idle
        if (taskPool->checkOver()) {
            // TODO: handle resp and exit
            // LOG() << "second all threads are idle." << std::endl;
            return handleSingleExecute(enum_start);
        }
        if (TaskSlot::g_args->overtime.load()) {
            // LOG() << "Query Time out." << std::endl;
            taskPool->cv.notify_all();
            return handleSingleExecute(enum_start);
        }
        // new task is made, continue to try getting task
        taskPool->subIdle();
    }
    return handleSingleExecute(enum_start);
    // LOG() << "Out singleExecute." << std::endl;
}

void
EvaluateQuery::handleTaskResp(int64_t l_embedding_count, ul l_call_count, std::chrono::steady_clock::time_point& time) {
    // for single thread statistics
    // add counts
    // LOG() << "add l_embedding_count = " << l_embedding_count << std::endl;
    // LOG() << "add call_count = " << l_call_count << std::endl;
    TaskSlot::g_args->embedding_count += l_embedding_count;
    TaskSlot::g_args->call_count += l_call_count;
    TaskSlot::g_args->addEmbeddingCount(std::this_thread::get_id(), l_embedding_count);
    TaskSlot::g_args->addCallCount(std::this_thread::get_id(), l_call_count);
    auto elapsed_exec = std::chrono::steady_clock::now() - time;
    // add exec time
    TaskSlot::g_args->addTimeExec(
        std::this_thread::get_id(), std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_exec).count()
    );
    time = std::chrono::steady_clock::now();
    // LOG() << "Finish a local task with " << l_embedding_count << " embeddings." << std::endl;
}

void
EvaluateQuery::resetExeTime(std::chrono::steady_clock::time_point& time) {
    auto elapsed_exec = std::chrono::steady_clock::now() - time;
    // add exec time
    TaskSlot::g_args->addTimeExec(
        std::this_thread::get_id(), std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_exec).count()
    );
    time = std::chrono::steady_clock::now();
}

void
EvaluateQuery::LFTJ(TaskPool* taskPool, TaskSlot& task, int task_id) {
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* idx_count = task.l_args->idx_count;
    ui** valid_candidate_idx = task.l_args->valid_candidate_idx;
    ui* embedding = task.l_args->embedding;
    ui* idx_embedding = task.l_args->idx_embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui** candidates = u_args->candidates;
    int max_depth = u_args->vertices_count;
    ui* order = u_args->order;
    // oss << "u's order = ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << new2olds[order[i]] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    /*
    NOTE: cur_depth is for
        idx[cur_depth], idx_count[cur_depth], valid_candidate_idx[cur_depth][idx[cur_depth]],
        order[cur_depth]
    */

    /*
    NOTE: u is for
        embedding[u], idx_embedding[u], visited_vertices[u], candidates[u],
        candidates_count[u]
    */
    // VertexID start_u = order[cur_depth];
    idx[cur_depth] = 0;
    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            // LOG() << "task_id == " << task_id << std::endl;
            ui valid_idx = valid_candidate_idx[cur_depth][idx[cur_depth]];
            VertexID u = order[cur_depth];
            VertexID v = candidates[u][valid_idx];
            l_call_count += 1;

            // std::ostringstream oss;
            // oss << "cur_depth == " << cur_depth << ", u == " << u << ", now matching v == "
            //     << candidates[u][valid_candidate_idx[cur_depth][idx[cur_depth]]] << std::endl;
            // oss << "valid_candidates: ";
            // for (ui i = 0; i < idx_count[cur_depth]; i++) {
            //     oss << candidates[u][valid_candidate_idx[cur_depth][i]] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            if (visited_vertices[v]) {
                idx[cur_depth] += 1;
                continue;
            }

            // match u to v
            embedding[u] = v;
            idx_embedding[u] = valid_idx;
            visited_vertices[v] = true;
            idx[cur_depth] += 1;

            if (taskPool->checkOverTime()) {
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                return;
            }

            if (cur_depth == max_depth - 1) {
                l_embedding_count += 1;

                if (TaskSlot::g_args->splitMode == Q) {
                    // std::ostringstream osst;
                    // osst << "embedding: ";
                    // for (int i = 0; i < max_depth; i++) {
                    //     osst << embedding[i] << ", ";
                    // }
                    // osst << std::endl;

                    // LOG() << "embedding: ";
                    // for (int i = 0; i < max_depth; i++) {
                    //     LOG() << embedding[i] << ", ";
                    // }
                    // LOG() << std::endl;

                    // LOG() << osst.str();
                    u_args->addPartialMatch(embedding);
                }
                // std::ostringstream oss;
                // oss << "get an embedding: ";
                // for (int i = 0; i < max_depth; i++) {
                //     oss<< embedding[i] << ", ";
                // }
                // oss << std::endl;
                // LOG() << oss.str();
                // oss.str("");

                // TODO: 实现partialResult类，高效保存partial matching，并且在这里实现join方法
                // saveEmbedding();
                visited_vertices[v] = false;
                if (l_embedding_count >= l_limit_num) {
                    // TODO: handleResponse();
                    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                    return;
                }
            } else {
                // go to the next level
                cur_depth += 1;
                idx[cur_depth] = 0;
                generateValidCandidateIndex(cur_depth, task);
            }
        }
        cur_depth -= 1;
        if (cur_depth < upper_depth) {
            break; // 结束整个匹配过程
        }
        VertexID u = order[cur_depth];
        visited_vertices[embedding[u]] = false;
        /*  NOTE: split here when backtracking
            For split_Q methods or thread_num == 1, no need to further split a Unit's task.
            There is only one task for a unit(temporary).
        */
        // LOG() << "cd split task." << std::endl;
        /**
         * 应该在split之前加时间，之后重置时间，现在其实是有冗余操作，先这样
         * */
        // resetExeTime(start_time_exec);
        taskPool->splitTask(task, cur_depth);
        // resetExeTime(start_time_exec);
        // LOG() << "over split task." << std::endl;
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
    return;
}

// TODO: opt to three parts
void  // 4 parts: up-down,up-x,x-down,x-x; down&x 2 parts so far
EvaluateQuery::bsxEnumerate4Parts(ui **&sep_flags, const VertexID* nodes, ui nodes_num,
                                   std::vector<std::vector<VertexID>>& cans, bool *&visited_v,
                                   uint64_t &cur_cnt, ui* idx, ui* cnt, ui* un_con_cnt, uint64_t* embedding_level) {
    ui depth = 0;
    idx[depth] = 0;
    cnt[depth] = sep_flags[nodes[depth]][0] < cans[nodes[depth]].size()
                 ? sep_flags[nodes[depth]][0] + 1 : cans[nodes[depth]].size();
    un_con_cnt[depth] = 0;
    // std::cout << "enumerate nodes: ";
    // for (auto& tmp_ele:nodes) std::cout << tmp_ele << ", ";
    // std::cout << std::endl;
    // std::cout << "sep_flags: ";
    // for (ui i = 0; i < nodes_num; i++) std::cout << sep_flags[nodes[i]][0] << ", ";
    // std::cout << std::endl;
    while (true) {
        while (idx[depth] < cnt[depth]) {
            // std::cout << "enumerate depth: " << depth << std::endl;
            // LOG() << "depth == " << depth << ", cnt[depth] == " << cnt[depth] << ", idx[depth] == " << idx[depth] << std::endl;
            auto u_idx = nodes[depth];
            VertexID& v = cans[u_idx][idx[depth]];
            ui& cur_sep = sep_flags[u_idx][0];
            // std::cout << "u_" << u << ", v_" << v << std::endl;
            if (depth == nodes_num - 1) {
                ui tmp_cnt = 0;
                for (ui i = 0; i < cans[u_idx].size(); i++) {
                    if (!visited_v[cans[u_idx][i]]) tmp_cnt++;
                }
                // LOG() << "depth == nodes_num - 1, tmp_cnt == " << tmp_cnt << std::endl;
                embedding_level[depth] += tmp_cnt;
                break;
            } else {
                idx[depth]++;
                if (idx[depth] > cur_sep) {
                    for (ui i = cur_sep; i < cans[u_idx].size(); i++) {
                        auto& can = cans[u_idx][i];
                        if (!visited_v[can]) un_con_cnt[depth]++;
                    }
                    if (un_con_cnt[depth] == 0) break;
                } else {
                    if (visited_v[v]) continue;
                    visited_v[v] = true;
                    // std::cout << "visited_v[" << v << "] = true" << std::endl;
                }
                depth++;
                idx[depth] = 0;
                cnt[depth] = sep_flags[nodes[depth]][0] + 1;
                embedding_level[depth] = 0;
                un_con_cnt[depth] = 0;
            }
        }
        depth--;
        if (depth == (ui)-1) {
            break;
        }
        if (idx[depth] > sep_flags[nodes[depth]][0]) {
            // process nodes which will not conflict downward
            embedding_level[depth+1] *= un_con_cnt[depth];
        } else {
            // process visited_v
            VertexID& v = cans[nodes[depth]][idx[depth]-1];
            visited_v[v] = false;
        }
        embedding_level[depth] += embedding_level[depth+1];
        // LOG() << "backtrack, embedding_level[" << depth << "] == " << embedding_level[depth] << std::endl;
        // std::cout << "embedding_level[" << depth << "]: " << embedding_level[depth] << std::endl;
        // std::cout << "embedding_level[" << depth+1 << "]: " << embedding_level[depth+1] << std::endl;
    }

    cur_cnt = embedding_level[0];
    return;
}

// according to indep_con_cnt info, seperate v_cans into two parts, return the #first_part(true)
ui
EvaluateQuery::bsxSepDiff(std::vector<VertexID> &v_cans, const ui *indep_con_cnt, int forward_idx, int backward_idx) {
    if (backward_idx-forward_idx == 0) return indep_con_cnt[v_cans[forward_idx]] != 0;
    ui first_con_cnt = indep_con_cnt[v_cans[forward_idx]];
    VertexID first_idx = v_cans[forward_idx];
    while(forward_idx < backward_idx) {
        while(forward_idx < backward_idx && !indep_con_cnt[v_cans[backward_idx]]) backward_idx--;
        if (forward_idx < backward_idx)
            v_cans[forward_idx++] = v_cans[backward_idx];
        while(forward_idx < backward_idx && indep_con_cnt[v_cans[forward_idx]]) forward_idx++;
        if (forward_idx < backward_idx)
            v_cans[backward_idx--] = v_cans[forward_idx];
    }
    v_cans[forward_idx] = first_idx;
    if (first_con_cnt) forward_idx++;
    return forward_idx;
}

void
EvaluateQuery::bsxGenResult(TaskSlot& task, ui *indep_con_cnt, ui** sep_flag, ui* idx, ui* cnt, ui* un_con_cnt, uint64_t* embedding_level) {
    // auto& visited_v = index.visited_v;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[task.l_args->id_unit];
    const Graph* q_graph = u_args->query_graph->subgraph;
    auto& visited_v = task.l_args->visited_vertices;
    // auto& indep_con_cnt = index.indep_con_cnt_;
    // auto& embedding_cnt = index.level_embeddings_;
    uint64_t embedding_cnt = 1;
    // auto& label_embeddings = index.label_embeddings_;
    uint64_t label_embeddings = 0;
    ui qnum = q_graph->getVerticesCount();
    ui label_num = q_graph->getLabelsCount();
    std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
    ui* order = u_args->order;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>>& FUValidCandidateIdx = task.l_args->FUValidCandidateIdx;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui>>& FUValidCandidatesCount = task.l_args->FUValidCandidatesCount;
    ui** candidates = u_args->candidates;
    ui* embedding = task.l_args->embedding;

    // generate valid_cans of indeps
    std::vector<std::vector<VertexID>> cans;
    cans.resize(qnum);

    for (ui u = 0; u < qnum; ++u) {
        if (FU_fns.find(u) != FU_fns.end()) {
            ui lastfnu = FU_fns[u].back();
            cans[u].reserve(FUValidCandidatesCount[u][lastfnu]);
            for (ui j = 0; j < FUValidCandidatesCount[u][lastfnu]; j++) {
                ui valid_idx = FUValidCandidateIdx[u][lastfnu][j];
                VertexID v = candidates[u][valid_idx];
                cans[u].emplace_back(v);
            }
        } else {
            cans[u].emplace_back(embedding[u]);
            visited_v[embedding[u]] = false;
        }
    }

    // std::ostringstream oss;
    // oss << "log cans: ";
    // for (ui i = 0; i < qnum; i++) {
    //     oss << "u_" << i << ": ";
    //     for (auto& can:cans[i]) oss << can << ", ";
    //     oss << std::endl;
    // }
    // LOG() << oss.str();
    // oss.str("");

    // LOG() << "label_num == " << label_num << std::endl;

    // NOTE: circinus 这里已经 refine 干净了，不需要生成独立集的东西了
    // if (bsxGenIndepValidCans(indep_num, indep, index, cans) == false) return;
    // std::cout << "valid_cans of indep::" << std::endl;
    // for (ui i = 0; i < qnum; i++) {
    //     std::cout << "u_" << i << " : ";
    //     for (auto& indep_can:cans[i]) std::cout << indep_can << ", ";
    //     std::cout << std::endl;
    // }
    // 这里要改，只要拿frozen_u就好了
    for (ui l_idx = 0; l_idx < label_num; l_idx++) {
        ui nodes_num;
        // these nodes have the same label
        auto nodes = q_graph->getVerticesByLabel(l_idx, nodes_num);
        // LOG() << "l_idx == " << l_idx << ", nodes_num == " << nodes_num << std::endl;
        // oss << "nodes: ";
        // for (ui i = 0; i < nodes_num; i++) oss << nodes[i] << ", ";
        // oss << std::endl;
        // LOG() << oss.str();
        // oss.str("");
        if (nodes_num == 0) continue;
        // compute the number of valid embedding
        // 1.By intersected, compute the cans which may conflict with others
        // 2.Based on conflict info, seperate cans into two part
        //   con: may conflict with others nodes, process as a backtracking
        //   un-con: will not conflict with others, use (#un-con) * (#embeddings of down-level)
        //   con&un-con->all need process visited_v
        // 3.Order does not matter in this backtracking, and will not influence up-level
        // ** because there is no great idea to process up-coflict nodes,
        //    we do not seperate nodes base on up-conlict
        if (nodes_num == 1) {  // the order of indep are always the same
            embedding_cnt *= cans[nodes[0]].size();
            continue;
        }
        // std::cout << "#" << nodes_num << ", " << "enumerate nodes : ";
        // for (ui i = 0; i < nodes_num; i++) std::cout << nodes[i] << ", ";
        // std::cout << std::endl;
        // 1.first scan, compute upward conflict
        for (ui i = 0; i < nodes_num; i++) {
            auto& node = nodes[i];
            auto& v_cans = cans[node];
            // do not seperate nodes base on up-conlict
            // auto& upward_sep = sep_flag[cur_idx][1];
            // auto v_cans_cnt = v_cans.size();
            // int forward_idx = 0;
            // int backward_idx = v_cans_cnt - 1;  // backward_idx may be -1
            // upward_sep = bsxSepDiff(v_cans, indep_con_cnt, forward_idx, backward_idx);
            for (auto v_can:v_cans) indep_con_cnt[v_can]++;
        }
        // 2.second scan, compute downward conflict
        for (ui i = 0; i < nodes_num; i++) {
            const ui &node = nodes[i];
            std::vector<VertexID> &v_cans = cans[node];
            ui &downward_sep0 = sep_flag[node][0];  // 0->sep the up-conflicts
            // auto& downward_sep1 = sep_flag[cur_idx][2];  // 1->sep the up-uncon.
            std::size_t v_cans_cnt = v_cans.size();  // assert(v_cans_cnt > 0) check at cans generation
            int forward_idx = 0;
            // int middle_idx = sep_flag[cur_idx][1];
            // int backward_idx = v_cans_cnt - 1;
            // std::cout << "indep_con_cnt: ";
            for (VertexID v_can:v_cans) indep_con_cnt[v_can]--;// std::cout <<" [" << v_can << "]: " << indep_con_cnt[v_can] << ", ";
            downward_sep0 = bsxSepDiff(v_cans, indep_con_cnt, forward_idx, v_cans_cnt - 1);
            // LOG() << "node == " << node << ", v_cans.size == " << v_cans.size() << ", downward_sep0 == " << downward_sep0 << std::endl;
            // LOG() << "node == " << node << ", downward_sep0 == " << downward_sep0 << std::endl;
            // std::cout << "sep: " << downward_sep0;
            // std::cout << std::endl;
            // downward_sep1 = bsxSepDiff(v_cans, indep_con_cnt, middle_idx, backward_idx);
        }
        // 3.enumerate the nodes based on diff features of 4 parts
        // ** just 2 parts so far
        memset(embedding_level, 0, sizeof(ui) * nodes_num);
        bsxEnumerate4Parts(sep_flag, nodes, nodes_num, cans, visited_v, label_embeddings, idx, cnt, un_con_cnt, embedding_level);
        embedding_cnt *= label_embeddings;
        if (embedding_cnt == 0) {
            // LOG() << "0 result" << ", l_idx == " << l_idx << std::endl;
            for (ui u = 0; u < FU_fns.size(); ++u) {
                if (FU_fns.find(u) == FU_fns.end()) {
                    visited_v[embedding[u]] = true;
                }
            }
            return;
        }
    }

    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    l_embedding_count += embedding_cnt;
    for (ui u = 0; u < FU_fns.size(); ++u) {
        if (FU_fns.find(u) == FU_fns.end()) {
            visited_v[embedding[u]] = true;
        }
    }
    return;
}

void
EvaluateQuery::exploreCircinusStyle(TaskPool* taskPool, TaskSlot& task, int task_id) {
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* idx_count = task.l_args->idx_count;
    ui** valid_candidate_idx = task.l_args->valid_candidate_idx;
    ui* embedding = task.l_args->embedding;
    ui* idx_embedding = task.l_args->idx_embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui** candidates = u_args->candidates;
    int max_depth = u_args->vertices_count;
    ui* order = u_args->order;
    ui* u_depth = u_args->u_depth;
    // ui* tree_order = u_args->tree_order;

    std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
    std::unordered_map<VertexID, std::unordered_map<VertexID, VertexID>>& preu = u_args->preu;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>>& FUValidCandidateIdx = task.l_args->FUValidCandidateIdx;
    std::unordered_map<VertexID, std::unordered_map<VertexID, ui>>& FUValidCandidatesCount = task.l_args->FUValidCandidatesCount;

    /**
     * 这一段都需要加入到初始化和分解里面，因为这个是要复制的
     */

    // 在NVC的可以冻结
    ui* frozen_depthes = new ui[u_args->vertices_count];
    ui frozen_count = 0;
    ui* fidx = new ui[u_args->vertices_count];

    for (int i = upper_depth; i < max_depth; ++i) {
        VertexID u = order[i];
        u_depth[u] = i;
        if (FU_fns.find(u) != FU_fns.end()) {
            frozen_depthes[frozen_count++] = i;
        }
    }

    // for bsxGenerate
    ui* bsxidx = new ui[u_args->vertices_count];
    ui* cnt = new ui[u_args->vertices_count];
    ui* un_con_cnt = new ui[u_args->vertices_count];
    uint64_t* embedding_level = new uint64_t[u_args->vertices_count]();

    ui *indep_con_cnt = new ui[TaskSlot::g_args->data_graph->getVerticesCount()]();
    // auto& sep_flag = index.sep_flag_;  // indexed by idx
    ui** sep_flag = new ui*[u_args->vertices_count];
    for (ui i = 0; i < u_args->vertices_count; i++) {
        sep_flag[i] = new ui[3]();
    }

    std::ostringstream oss;
    // oss << "Output order : " << std::endl;
    // for (int i = 0; i < max_depth; i++) {
    //     oss << order[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "log frozen_depthes : ";
    // for (ui i = 0; i < frozen_count; i++) {
    //     oss << frozen_depthes[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "log FU_fns : " << std::endl;
    // for (const auto& [fu, fns] : FU_fns) {
    //     oss << "fu = " << fu << ", fns = ";
    //     for (VertexID fn : fns) {
    //         oss << fn << ", ";
    //     }
    //     oss << std::endl;
    // }
    // LOG() << oss.str();
    // oss.str("");

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

    /*
    NOTE: cur_depth is for
        idx[cur_depth], idx_count[cur_depth], valid_candidate_idx[cur_depth][idx[cur_depth]],
        order[cur_depth]
    */

    /*
    NOTE: u is for
        embedding[u], idx_embedding[u], visited_vertices[u], candidates[u],
        candidates_count[u]
    */
    // VertexID start_u = order[cur_depth];
    idx[cur_depth] = 0;

    // if (cur_depth == 0 && FU_fns.find(order[cur_depth]) != FU_fns.end()) {
    //     // LOG() << "u" << u << " should be frozen." << std::endl;
    //     idx[cur_depth] = idx_count[cur_depth];
    //     // go to the next level
    //     // 所有的求交函数都要变，用的都是FUValidCandidateIdx了，那么所有frozen_u对应的往下的Edges是需要额外维护的

    //     // NOTE: 这里要设置 FUValidCandidateIdx[fu][fu] = valid_candidate_idx[cur_depth]，也就是他本身
    //     std::memcpy(FUValidCandidateIdx[order[cur_depth]][order[cur_depth]], valid_candidate_idx[cur_depth], sizeof(ui) * idx_count[cur_depth]);
    //     FUValidCandidatesCount[order[cur_depth]][order[cur_depth]] = idx_count[cur_depth];
    //     idx[cur_depth] = idx_count[cur_depth];
    // }
    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            VertexID u = order[cur_depth];

            // oss << "cur_depth == " << cur_depth << ", u == " << u << ", now matching v == "
            //     << candidates[u][valid_candidate_idx[cur_depth][idx[cur_depth]]] << std::endl;
            // oss << "valid_candidates: ";
            // for (ui i = 0; i < idx_count[cur_depth]; i++) {
            //     oss << candidates[u][valid_candidate_idx[cur_depth][i]] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");


            // LOG() << "depth " << cur_depth << " task.l_args->FUValidCandidateIdx[9][9] : " << task.l_args->FUValidCandidateIdx[9][9] << std::endl; 
            /*
                NOTE: 遇到冻结的需要特殊处理，直接到下一层，并且回溯的时候也要跳过这一层直接到上一层
            */
            if (taskPool->checkOverTime()) {
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);

                delete[] bsxidx;
                delete[] cnt;
                delete[] un_con_cnt;
                delete[] embedding_level;
                delete[] indep_con_cnt;
                for (ui i = 0; i < u_args->vertices_count; i++) {
                    delete[] sep_flag[i];
                }
                delete[] sep_flag;
                delete [] frozen_depthes;
                delete [] fidx;

                return;
            }
            if (FU_fns.find(u) != FU_fns.end(u)) {
                // LOG() << "u" << u << " should be frozen." << std::endl;
                idx[cur_depth] = idx_count[cur_depth];
                // go to the next level
                // 所有的求交函数都要变，用的都是 FUValidCandidateIdx 了，那么所有frozen_u对应的往下的Edges是需要额外维护的

                // NOTE: 这里要设置 FUValidCandidateIdx[fu][fu] = valid_candidate_idx[cur_depth]，也就是他本身
                std::memcpy(FUValidCandidateIdx[u][u], valid_candidate_idx[cur_depth], sizeof(ui) * idx_count[cur_depth]);
                FUValidCandidatesCount[u][u] = idx_count[cur_depth];

                if (cur_depth == max_depth - 1) {

                    // if (TaskSlot::g_args->splitMode == Q || TaskSlot::g_args->splitMode == Q_C) {
                    //     u_args->addPartialMatch(embedding);
                    // }
                    // TODO: 实现partialResult类，高效保存partial matching，并且在这里实现join方法
                    // saveEmbedding();

                    /*
                        
                        1. 把 FU_fns 的 valid_candidate_idx 算出来
                        2. 做笛卡尔积，要注意去重，其实也不能算笛卡尔积，而是小范围的backtracking
                        3. 设置一个skipIdx，在这些地方是要跳过的，但是每次这层回溯之后，就要置为空
                    */
                    // for (VertexID frozen_u : FU_fns) {
                    //     /*
                    //         这个函数要改造一下，不是到了最后一层才算，而是每一层都把关联的backward_frozen_u的valid_candi_idx更新一下
                    //     */
                    //     updateFrozenCandidateIndex(u_depth[frozen_u], task, FUValidCandidateIdx, FUValidCandidatesCount);
                    // }
                    // LOG() << "frozen_count == " << frozen_count << std::endl;
                    if (frozen_count == 0) {
                        l_embedding_count += 1;
                        if (TaskSlot::g_args->splitMode == Q) {
                            u_args->addPartialMatch(embedding);
                        }
                    } else {
                        bsxGenResult(task, indep_con_cnt, sep_flag, bsxidx, cnt, un_con_cnt, embedding_level);
                        if (taskPool->checkOverTime()) {
                            handleTaskResp(l_embedding_count, l_call_count, start_time_exec);

                            delete[] bsxidx;
                            delete[] cnt;
                            delete[] un_con_cnt;
                            delete[] embedding_level;
                            delete[] indep_con_cnt;
                            for (ui i = 0; i < u_args->vertices_count; i++) {
                                delete[] sep_flag[i];
                            }
                            delete[] sep_flag;
                            delete [] frozen_depthes;
                            delete [] fidx;

                            return;
                        } 
                    }

                    // 这里需要额外处理，max_depth可能是最后一层
                    // visited_vertices[v] = false;
                    if (l_embedding_count >= l_limit_num) {
                        // TODO: handleResponse();
                        handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                        return;
                    }
                } else {
                    // go to the next level
                    cur_depth += 1;
                    idx[cur_depth] = 0;
                    // generateValidCandidateIndex(cur_depth, task);
                    generateValidCandidateIndexUseUnion(cur_depth, task);
                    // bool emptyFlag = false;
                    // refineFUVCIS(cur_depth, task, emptyFlag);
                }

            } else {
                ui valid_idx = valid_candidate_idx[cur_depth][idx[cur_depth]];
                VertexID v = candidates[u][valid_idx];
                // LOG() << "cur_depth == " << cur_depth << ", u == " << u << ", idx[cur_depth] == " << idx[cur_depth] << ", valid_idx == " << valid_idx << ", v == " << v << std::endl;
                l_call_count += 1;

                if (visited_vertices[v]) {
                    idx[cur_depth] += 1;
                    continue;
                }

                // match u to v
                embedding[u] = v;
                idx_embedding[u] = valid_idx;
                visited_vertices[v] = true;
                idx[cur_depth] += 1;

                bool emptyFlag = false;
                refineFUVCIS(cur_depth, task, emptyFlag);

                if (emptyFlag) {
                    idx[cur_depth] += 1;
                    continue;
                }

                if (cur_depth == max_depth - 1) {
                    // if (TaskSlot::g_args->splitMode == Q || TaskSlot::g_args->splitMode == Q_C) {
                    //     u_args->addPartialMatch(embedding);
                    // }
                    // TODO: 实现partialResult类，高效保存partial matching，并且在这里实现join方法
                    // saveEmbedding();

                    /*
                        
                        1. 把 FU_fns 的 valid_candidate_idx 算出来
                        2. 做笛卡尔积，要注意去重，其实也不能算笛卡尔积，而是小范围的backtracking
                        3. 设置一个skipIdx，在这些地方是要跳过的，但是每次这层回溯之后，就要置为空
                    */
                    // for (VertexID frozen_u : FU_fns) {
                    //     /*
                    //         这个函数要改造一下，不是到了最后一层才算，而是每一层都把关联的backward_frozen_u的valid_candi_idx更新一下
                    //     */
                    //     updateFrozenCandidateIndex(u_depth[frozen_u], task, FUValidCandidateIdx, FUValidCandidatesCount);
                    // }
                    // LOG() << "frozen_count == " << frozen_count << std::endl;
                    if (frozen_count == 0) {
                        l_embedding_count += 1;
                    if (TaskSlot::g_args->splitMode == Q) {
                        u_args->addPartialMatch(embedding);
                    }
                    } else {
                        bsxGenResult(task, indep_con_cnt, sep_flag, bsxidx, cnt, un_con_cnt, embedding_level);
                        if (taskPool->checkOverTime()) {
                            handleTaskResp(l_embedding_count, l_call_count, start_time_exec);

                            delete[] bsxidx;
                            delete[] cnt;
                            delete[] un_con_cnt;
                            delete[] embedding_level;
                            delete[] indep_con_cnt;
                            for (ui i = 0; i < u_args->vertices_count; i++) {
                                delete[] sep_flag[i];
                            }
                            delete[] sep_flag;
                            delete [] frozen_depthes;
                            delete [] fidx;

                            return;
                        } 
                    }

                    // 这里需要额外处理，max_depth可能是最后一层
                    visited_vertices[v] = false;
                    if (l_embedding_count >= l_limit_num) {
                        // TODO: handleResponse();
                        handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                        return;
                    }
                } else {
                    // go to the next level
                    cur_depth += 1;
                    idx[cur_depth] = 0;
                    // generateValidCandidateIndex(cur_depth, task);
                    generateValidCandidateIndexUseUnion(cur_depth, task);
                }
            }
        }
        cur_depth -= 1;
        while(FU_fns.find(order[cur_depth]) != FU_fns.end()) {
            cur_depth -= 1;
        }
        if (cur_depth < upper_depth) {
            break; // 结束整个匹配过程
        }
        VertexID u = order[cur_depth];
        visited_vertices[embedding[u]] = false;
        /*  NOTE: split here when backtracking
            For split_Q methods or thread_num == 1, no need to further split a Unit's task.
            There is only one task for a unit(temporary).
        */
        bool has_frozen = false;
        for(int tempdetph = upper_depth; tempdetph < cur_depth; tempdetph++) {
            if (FU_fns.find(order[tempdetph]) != FU_fns.end()) {
                has_frozen = true;
                break;
            }
        }
        // if (!has_frozen) {
        //     // resetExeTime(start_time_exec);
        //     taskPool->splitTask(task, cur_depth);
        //     // resetExeTime(start_time_exec);
        // }
        if (1) {
            // resetExeTime(start_time_exec);
            taskPool->splitTask(task, cur_depth);
            // resetExeTime(start_time_exec);
        }
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);

    delete[] bsxidx;
    delete[] cnt;
    delete[] un_con_cnt;
    delete[] embedding_level;
    delete[] indep_con_cnt;
    for (ui i = 0; i < u_args->vertices_count; i++) {
        delete[] sep_flag[i];
    }
    delete[] sep_flag;

    delete [] frozen_depthes;
    delete [] fidx;

    return;
}

void
EvaluateQuery::exploreGraphQLStyle(TaskPool* taskPool, TaskSlot& task, int task_id) {
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* idx_count = task.l_args->idx_count;
    // 这个是graphQL专属的，直接存储valid_candidate，而不是idx
    ui** valid_candidate = task.l_args->valid_candidate;
    ui* embedding = task.l_args->embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    // LOG() << "upper_depth == " << cur_depth << ", idx_count == " << idx_count[cur_depth] << std::endl;
    // LOG() << "idx[upper_depth] == " << idx[cur_depth] << std::endl;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    int max_depth = u_args->vertices_count;
    // LOG() << "max_depth == " << max_depth << std::endl;
    ui* order = u_args->order;
    const Graph* data_graph = TaskSlot::g_args->data_graph;
    
    idx[cur_depth] = 0;
    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            VertexID u = order[cur_depth];
            VertexID v = valid_candidate[cur_depth][idx[cur_depth]];
            embedding[u] = v;
            l_call_count += 1;
            // LOG() << "add a match : " << u << " -> " << v << std::endl;
            visited_vertices[v] = true;
            idx[cur_depth] += 1;

            if (taskPool->checkOverTime()) {
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                return;
            }

            if (cur_depth == max_depth - 1) {
                l_embedding_count += 1;
                if (TaskSlot::g_args->splitMode == Q) {
                    u_args->addPartialMatch(embedding);
                }
                visited_vertices[v] = false;
                if (l_embedding_count >= l_limit_num) {
                    // TODO: handleResponse();
                    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                    return;
                }
            } else {
                cur_depth += 1;
                idx[cur_depth] = 0;
                generateValidCandidates(data_graph, cur_depth, task);
            }
        }

        cur_depth -= 1;
        if (cur_depth < upper_depth)
            break;
        else
            visited_vertices[embedding[order[cur_depth]]] = false;

        // resetExeTime(start_time_exec);
        taskPool->splitTask(task, cur_depth);
        // resetExeTime(start_time_exec);
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
}

void
EvaluateQuery::exploreGraph(TaskPool* taskPool, TaskSlot& task, int task_id) {
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* idx_count = task.l_args->idx_count;
    ui** valid_candidate_idx = task.l_args->valid_candidate_idx;
    ui* embedding = task.l_args->embedding;
    ui* idx_embedding = task.l_args->idx_embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui** candidates = u_args->candidates;
    int max_depth = u_args->vertices_count;
    ui* order = u_args->order;

    const Graph* data_graph = TaskSlot::g_args->data_graph;   

    idx[cur_depth] = 0;
    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            // LOG() << "cur_depth == " << cur_depth << ", idx_count == " << idx_count[cur_depth] << std::endl;
            ui valid_idx = valid_candidate_idx[cur_depth][idx[cur_depth]];
            VertexID u = order[cur_depth];
            VertexID v = candidates[u][valid_idx];

            embedding[u] = v;
            l_call_count += 1;
            idx_embedding[u] = valid_idx;
            visited_vertices[v] = true;
            idx[cur_depth] += 1;

            if (taskPool->checkOverTime()) {
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                return;
            }

            if (cur_depth == max_depth - 1) {
                l_embedding_count += 1;
                if (TaskSlot::g_args->splitMode == Q) {
                    u_args->addPartialMatch(embedding);
                }
                visited_vertices[v] = false;
                if (l_embedding_count >= l_limit_num) {
                    // TODO: handleResponse();
                    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                    return;
                }
            } else {
                cur_depth += 1;
                idx[cur_depth] = 0;
                // CFL里只用到了pivot和下面一个点的edge，其他的还是向bn检查边连接性
                generateValidCandidateIndexWithPivot(data_graph, cur_depth, task);
            }
        }

        cur_depth -= 1;
        if (cur_depth < upper_depth)
            break;
        else
            visited_vertices[embedding[order[cur_depth]]] = false;

        // resetExeTime(start_time_exec);
        taskPool->splitTask(task, cur_depth);
        // resetExeTime(start_time_exec);
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
    return;
}

void
EvaluateQuery::exploreDPisoStyle(TaskPool* taskPool, TaskSlot& task, int task_id) {
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* u_idx_count = task.l_args->u_idx_count;
    ui** valid_candidate_idx = task.l_args->valid_candidate_idx;
    ui* embedding = task.l_args->embedding;
    ui* idx_embedding = task.l_args->idx_embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;
    ui* extendable = task.l_args->extendable;
    std::vector<dpiso_min_pq>& vec_rank_queue = task.l_args->vec_rank_queue;
    ui* l_order = task.l_args->l_order;
    std::unordered_set<ui>& splited_u_set = task.l_args->splited_u_set;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui** candidates = u_args->candidates;
    ui* candidates_count = u_args->candidates_count;
    Edges*** edge_matrix = u_args->edge_matrix;
    int max_depth = u_args->vertices_count;
    ui* tree_order = u_args->tree_order;
    TreeNode* tree = u_args->tree;
    const Graph* query_graph = u_args->query_graph->subgraph;

    bool task_splited_flag = false;
    ui init_splited_u_set_size = splited_u_set.size();

// #ifdef ENABLE_FAILING_SET
std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> ancestors;
std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> vec_failing_set(max_depth);
std::unordered_map<VertexID, VertexID> reverse_embedding;
if (FAILING_SET_FLAG && !task_splited_flag) {
    computeAncestor(query_graph, tree, tree_order, ancestors);
    reverse_embedding.reserve(MAXIMUM_QUERY_GRAPH_SIZE * 2);
}
// #endif

// 默认第一层不存在 idx_count[u] == 0 的情况
// 这里的idx和idx_count还是针对u的，炸裂，很难搞了

    // print extendable
    // std::ostringstream oss;

    // oss << "Output tree_order : " << std::endl;
    // for (int i = 0; i < max_depth; i++) {
    //     oss << tree_order[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "New Task upper_depth == " << cur_depth << std::endl;
    // oss << "task extendable : ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << extendable[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "log upper_depth's valid_candidate_idx : ";
    // for (int i = 0; i < u_idx_count[l_order[upper_depth]]; i++) {
    //     oss << valid_candidate_idx[l_order[upper_depth]][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "log embedding : ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << embedding[i] << ", ";
    // }
    // oss << std::endl;
    // oss << "log idx_embedding : ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << idx_embedding[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "log l_order : ";
    // for (int i = 0; i <= upper_depth; i++) {
    //     oss << l_order[i] << ", ";
    // }
    // oss << std::endl;

    // oss << "log upper embedding : ";
    // for (int i = 0; i <= upper_depth; i++) {
    //     oss << embedding[l_order[i]] << ", ";
    // }

    VertexID u = l_order[cur_depth];
    idx[u] = 0;

    // if (cur_depth == 0 && vec_rank_queue.size() == 0) {
    //     vec_rank_queue.emplace_back(dpiso_min_pq(extendable_vertex_compare));
    // }

    // pretty print vec_rank_queue
    // std::ostringstream oss;
    // oss << "vec_rank_queue 'size == " << vec_rank_queue.size() << " : " << std::endl;
    // for (size_t i = 0; i < vec_rank_queue.size(); i++) {
    //     if (vec_rank_queue[i].empty()) {
    //         oss << "depth == " << i << ", empty, " << std::endl;
    //         continue;
    //     }
    //     oss << "depth == " << i << ": " << vec_rank_queue[i].top().first.first << std::endl;
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // oss.str("");
    // oss << "explore u of upper_depth == " << upper_depth << ", u == "  << u << std::endl;
    // oss << "explore u_idx_count of u's: ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << u_idx_count[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    while (true) {
        while (idx[u] < u_idx_count[u]) {
            ui valid_idx = valid_candidate_idx[u][idx[u]];
            assert(valid_idx < candidates_count[u]);
            VertexID v = candidates[u][valid_idx];

            if (visited_vertices[v]) {
                // LOG() << "visited, failed." << std::endl;
                idx[u] += 1;
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
                vec_failing_set[cur_depth] = ancestors[u];
                vec_failing_set[cur_depth] |= ancestors[reverse_embedding[v]];
                vec_failing_set[cur_depth - 1] |= vec_failing_set[cur_depth];
}
// #endif
                continue;
            }
            embedding[u] = v;
            l_call_count += 1;
            idx_embedding[u] = valid_idx;
            visited_vertices[v] = true;
            idx[u] += 1;

// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
            reverse_embedding[v] = u;
}
// #endif

            if (taskPool->checkOverTime()) {
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                return;
            }

/*
    NOTE: 这一段是完全根据原来的DPiso第一层用for循环来的。
    之所以要把第一层的for改到while里面，是因为我要split，搞在一起方便一点。
    但是感觉也可以照着第一层for循环写一下试试。
*/ 
            if (cur_depth == 0) {
                vec_rank_queue.emplace_back(dpiso_min_pq(extendable_vertex_compare));
                updateExtendableVertex(u, task, u_args);
                u = vec_rank_queue.back().top().first.first;
                vec_rank_queue.back().pop();

                if (splited_u_set.find(u) != splited_u_set.end()) {
                    VertexID*& temp_buffer = task.l_args->temp_buffer;
                    generateValidCandidateIndex(u, idx_embedding, u_idx_count, valid_candidate_idx[u], edge_matrix, tree[u].bn_,
                                                tree[u].bn_count_, temp_buffer);
                }    

                // LOG() << "vec_rank_queue[cur_depth].size == " << vec_rank_queue[cur_depth].size() << std::endl;
                // LOG() << "vec_rank_queue.top.u == " << vec_rank_queue.back().top().first.first << std::endl;

// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
                if (u_idx_count[u] == 0) {
                    // LOG() << "u_idx_count[u] == 0, u == " << u << std::endl;
                    vec_failing_set[cur_depth] = ancestors[u];
                } else {
                    vec_failing_set[cur_depth].reset();
                }
}
// #endif        

                cur_depth += 1;
                l_order[cur_depth] = u;
                idx[u] = 0;
                continue;
            }
            // else {
            //     assert(vec_rank_queue.size() > 0);
            // } 

            if (cur_depth == max_depth - 1) {
                l_embedding_count += 1;
                if (TaskSlot::g_args->splitMode == Q) {
                    u_args->addPartialMatch(embedding);
                }
                visited_vertices[v] = false;
                // oss << "get an embedding: ";
                // for (int i = 0; i < max_depth; i++) {
                //     oss<< embedding[i] << ", ";
                // }
                // oss << std::endl;
                // LOG() << oss.str();
                // oss.str("");
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
                reverse_embedding.erase(embedding[u]);
                vec_failing_set[cur_depth].set();
                vec_failing_set[cur_depth - 1] |= vec_failing_set[cur_depth];
}
// #endif
                if (l_embedding_count >= l_limit_num) {
                    // TODO: handleResponse();
                    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                    return;
                }
            } else {

                vec_rank_queue.emplace_back(vec_rank_queue.back());
                updateExtendableVertex(u, task, u_args);

                // test copy of pq
                // LOG() << "log vec_rank_queue[cur_depth -1] : ";

                // vec_rank_queue.back().pop();

                if (vec_rank_queue.back().empty()) {
                    // oss.str("");
                    // oss << "Error: vec_rank_queue is empty, " << "depth == " << cur_depth << "." << std::endl;
                    // oss << "l_ordered u's : ";
                    // for (int i = 0; i < max_depth; i++) {
                    //     oss << l_order[i] << ", ";
                    // }
                    // oss << std::endl;
                    // LOG() << oss.str();
                    // LOG() << "Error: vec_rank_queue is empty, " << "depth == " << cur_depth << "." << std::endl;
                    abort();
                }

                // LOG() << "vec_rank_queue[cur_depth].size == " << vec_rank_queue[cur_depth].size() << std::endl;
                // LOG() << "vec_rank_queue.top.u == " << vec_rank_queue.back().top().first.first << std::endl;

                u = vec_rank_queue.back().top().first.first;
                vec_rank_queue.back().pop();

                if (splited_u_set.find(u) != splited_u_set.end()) {
                    VertexID*& temp_buffer = task.l_args->temp_buffer;
                    generateValidCandidateIndex(u, idx_embedding, u_idx_count, valid_candidate_idx[u], edge_matrix, tree[u].bn_,
                                                tree[u].bn_count_, temp_buffer);
                }                
                
                cur_depth += 1;
                idx[u] = 0;
                l_order[cur_depth] = u;


// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
                if (u_idx_count[u] == 0) {
                    // LOG() << "u_idx_count[u] == 0, u == " << u << std::endl;
                    vec_failing_set[cur_depth - 1] = ancestors[u];
                } else {
                    vec_failing_set[cur_depth - 1].reset();
                }
}
// #endif
            }
        }

        cur_depth -= 1;
        if (cur_depth < upper_depth) {
            break;
            // LOG() << "break in idx[u] == " << idx[u] << std::endl;
        }

        // // 这里需要再生成一次cur_depth+1的u的valid_candidate_idx
        // u = l_order[cur_depth + 1];

        vec_rank_queue.pop_back();
        u = l_order[cur_depth];
        visited_vertices[embedding[u]] = false;
        restoreExtendableVertex(tree, u, extendable);
// #ifdef ENABLE_FAILING_SET
if (FAILING_SET_FLAG && !task_splited_flag) {
        reverse_embedding.erase(embedding[u]);
        if (cur_depth > 0) {
            if (!vec_failing_set[cur_depth].test(u)) {
                // 这里是失败集剪枝生效的地方
                vec_failing_set[cur_depth - 1] = vec_failing_set[cur_depth];
                idx[u] = u_idx_count[u];
            } else {
                vec_failing_set[cur_depth - 1] |= vec_failing_set[cur_depth];
            }
        }
}
// #endif
        // resetExeTime(start_time_exec);
        taskPool->splitTaskU(task, cur_depth);
        // resetExeTime(start_time_exec);
        if (init_splited_u_set_size < splited_u_set.size()) {
            task_splited_flag = true;
        }
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
    return;
}

void
EvaluateQuery::exploreCECIStyle(TaskPool* taskPool, TaskSlot& task, int task_id) {
    std::ostringstream oss;
    auto start_time_exec = std::chrono::steady_clock::now();

    ui* idx = task.l_args->idx;
    ui* idx_count = task.l_args->idx_count;
    ui* embedding = task.l_args->embedding;
    bool* visited_vertices = task.l_args->visited_vertices;
    int upper_depth = task.l_args->upper_depth;
    int cur_depth = task.l_args->upper_depth;
    int64_t& l_embedding_count = task.l_args->l_embedding_count;
    ul l_call_count = task.l_args->l_call_count;
    int64_t l_limit_num =  task.l_args->l_limit_num;
    int id_unit = task.l_args->id_unit;
    ui** valid_candidate = task.l_args->valid_candidate;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    int max_depth = u_args->vertices_count;
    ui* order = u_args->order;

    // oss << "order: ";
    // for (int i = 0; i < max_depth; i++) {
    //     oss << order[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    idx[cur_depth] = 0;
    while (true) {
        while (idx[cur_depth] < idx_count[cur_depth]) {
            VertexID u = order[cur_depth];
            VertexID v = valid_candidate[cur_depth][idx[cur_depth]];
            idx[cur_depth] += 1;

            // LOG() << "cur_depth == " << cur_depth << ", u == " << u << ", now matching v == " << v << std::endl;
            // oss << "valid_candidates: ";
            // for (ui i = 0; i < idx_count[cur_depth]; i++) {
            //     oss << valid_candidate[cur_depth][i] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();
            // oss.str("");

            if (visited_vertices[v]) {
                // LOG() << "u == " << u << ", v == " << v << " visited, failed." << std::endl;
                continue;
            }

            embedding[u] = v;
            l_call_count += 1;
            visited_vertices[v] = true;

            if (taskPool->checkOverTime()) {
                // LOG() << "Time out." << std::endl;
                handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                return;
            }

            if (cur_depth == max_depth - 1) {
                l_embedding_count += 1;
                if (TaskSlot::g_args->splitMode == Q) {
                    u_args->addPartialMatch(embedding);
                }
                visited_vertices[v] = false;
                if (l_embedding_count >= l_limit_num) {
                    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
                    return;
                }
            } else {
                cur_depth += 1;
                idx[cur_depth] = 0;
                generateValidCandidates(cur_depth, task);
                // LOG() << "u = " << order[cur_depth] << ", cur_depth = " << cur_depth << ", idx_count[cur_depth] = " << idx_count[cur_depth] << std::endl;
            }
        }

        cur_depth -= 1;
        if (cur_depth < upper_depth)
            break;
        else {
            VertexID u = order[cur_depth];
            visited_vertices[embedding[u]] = false;
        }
        // resetExeTime(start_time_exec);
        taskPool->splitTask(task, cur_depth);
        // resetExeTime(start_time_exec);
    }
    handleTaskResp(l_embedding_count, l_call_count, start_time_exec);
    return;
}

// void EvaluateQuery::generateBN(const Graph *query_graph, ui *order, ui *pivot, ui** bn, ui* bn_count) {
//     ui query_vertices_num = query_graph->getVerticesCount();
//     // bn_count = new ui[query_vertices_num];
//     std::fill(bn_count, bn_count + query_vertices_num, 0);

//     std::vector<bool> visited_vertices(query_vertices_num, false);
//     visited_vertices[order[0]] = true;
//     for (ui i = 1; i < query_vertices_num; ++i) {
//         VertexID vertex = order[i];

//         ui nbrs_cnt;
//         const ui *nbrs = query_graph->getVertexNeighbors(vertex, nbrs_cnt);
//         for (ui j = 0; j < nbrs_cnt; ++j) {
//             VertexID nbr = nbrs[j];

//             if (visited_vertices[nbr] && nbr != pivot[i]) {
//                 bn[i][bn_count[i]++] = nbr;
//             }
//         }

//         visited_vertices[vertex] = true;
//     }
// }

// // 分配内存改到持有该数组的类里面
// void EvaluateQuery::generateBN(const Graph *query_graph, ui *order, ui** bn, ui* bn_count) {
//     ui query_vertices_num = query_graph->getVerticesCount();
//     // bn_count = new ui[query_vertices_num];
//     std::fill(bn_count, bn_count + query_vertices_num, 0);
//     // bn = new ui *[query_vertices_num];
//     // for (ui i = 0; i < query_vertices_num; ++i) {
//     //     bn[i] = new ui[query_vertices_num];
//     // }

//     std::vector<bool> visited_vertices(query_vertices_num, false);
//     visited_vertices[order[0]] = true;
//     for (ui i = 1; i < query_vertices_num; ++i) {
//         VertexID vertex = order[i];

//         ui nbrs_cnt;
//         const ui *nbrs = query_graph->getVertexNeighbors(vertex, nbrs_cnt);
//         for (ui j = 0; j < nbrs_cnt; ++j) {
//             VertexID nbr = nbrs[j];

//             // LOG() << "i = " << i << ", bn_count[i] = " << bn_count[i] << std::endl;
//             if (visited_vertices[nbr]) {
//                 bn[i][bn_count[i]++] = nbr;
//             }
//         }

//         visited_vertices[vertex] = true;
//     }
// }

void EvaluateQuery::generateBN(UnitArgs* u_args) {
    Graph* query_graph = u_args->query_graph->subgraph;
    ui* order = u_args->order;
    ui** bn = u_args->bn;
    ui* bn_count = u_args->bn_count;
    ui query_vertices_num = query_graph->getVerticesCount();
    // bn_count = new ui[query_vertices_num];
    std::fill(bn_count, bn_count + query_vertices_num, 0);
    // bn = new ui *[query_vertices_num];
    // for (ui i = 0; i < query_vertices_num; ++i) {
    //     bn[i] = new ui[query_vertices_num];
    // }

    std::vector<bool> visited_vertices(query_vertices_num, false);
    visited_vertices[order[0]] = true;
    for (ui i = 1; i < query_vertices_num; ++i) {
        VertexID vertex = order[i];

        ui nbrs_cnt;
        const ui *nbrs = query_graph->getVertexNeighbors(vertex, nbrs_cnt);
        for (ui j = 0; j < nbrs_cnt; ++j) {
            VertexID nbr = nbrs[j];

            // LOG() << "i = " << i << ", bn_count[i] = " << bn_count[i] << std::endl;
            if (visited_vertices[nbr]) {
                bn[i][bn_count[i]++] = nbr;
            }
        }

        visited_vertices[vertex] = true;
    }
}

void EvaluateQuery::generateBNWithPivot(UnitArgs* u_args) {
    Graph* query_graph = u_args->query_graph->subgraph;
    ui* order = u_args->order;
    // std::ostringstream oss;
    // oss << "order : " << std::endl;
    // for (ui i = 0; i < u_args->vertices_count; i++) {
    //     oss << order[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    ui* pivot = u_args->pivots;
    ui** bn = u_args->bn;
    ui* bn_count = u_args->bn_count;
    ui query_vertices_num = query_graph->getVerticesCount();
    // LOG() << "query_vertices_num == " << query_vertices_num << std::endl;
    // bn_count = new ui[query_vertices_num];
    std::fill(bn_count, bn_count + query_vertices_num, 0);

    std::vector<bool> visited_vertices(query_vertices_num, false);
    visited_vertices[order[0]] = true;
    for (ui i = 1; i < query_vertices_num; ++i) {
        VertexID vertex = order[i];

        ui nbrs_cnt;
        const ui *nbrs = query_graph->getVertexNeighbors(vertex, nbrs_cnt);
        // LOG() << "nbrs_cnt == " << nbrs_cnt << std::endl;
        for (ui j = 0; j < nbrs_cnt; ++j) {
            VertexID nbr = nbrs[j];

            if (visited_vertices[nbr] && nbr != pivot[i]) {
                bn[i][bn_count[i]++] = nbr;
                // LOG() << "vertex == " << vertex << ", bn_count[i] == " << bn_count[i] << ", add nbr == " << nbr << std::endl;
            }
        }
        // LOG() << "i = " << i << ", bn_count[i] = " << bn_count[i] << std::endl;

        visited_vertices[vertex] = true;
    }
}

void EvaluateQuery::generateValidCandidateIndex(const Graph *data_graph, ui depth, ui *embedding, ui *idx_embedding,
                                                ui *idx_count, ui **valid_candidate_index, Edges ***edge_matrix,
                                                bool *visited_vertices, ui **bn, ui *bn_cnt, ui *order, ui *pivot,
                                                ui **candidates) {
    VertexID u = order[depth];
    VertexID pivot_vertex = pivot[depth];
    ui idx_id = idx_embedding[pivot_vertex];
    Edges &edge = *edge_matrix[pivot_vertex][u];
    ui count = edge.offset_[idx_id + 1] - edge.offset_[idx_id];
    ui *candidate_idx = edge.edge_ + edge.offset_[idx_id];

    ui valid_candidate_index_count = 0;

    if (bn_cnt[depth] == 0) {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v])
                valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
        }
    } else {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v]) {
                bool valid = true;

                for (ui j = 0; j < bn_cnt[depth]; ++j) {
                    VertexID u_bn = bn[depth][j];
                    VertexID u_bn_v = embedding[u_bn];

                    if (!data_graph->checkEdgeExistence(temp_v, u_bn_v)) {
                        valid = false;
                        break;
                    }
                }

                if (valid)
                    valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
            }
        }
    }

    idx_count[depth] = valid_candidate_index_count;
}

void EvaluateQuery::releaseBuffer(ui query_vertices_num, ui *idx, ui *idx_count, ui *embedding, ui *idx_embedding,
                                  ui *temp_buffer, ui **valid_candidate_idx, bool *visited_vertices, ui **bn,
                                  ui *bn_count) {
    delete[] idx;
    delete[] idx_count;
    delete[] embedding;
    delete[] idx_embedding;
    delete[] visited_vertices;
    delete[] bn_count;
    delete[] temp_buffer;
    for (ui i = 0; i < query_vertices_num; ++i) {
        delete[] valid_candidate_idx[i];
        delete[] bn[i];
    }

    delete[] valid_candidate_idx;
    delete[] bn;
}

void
EvaluateQuery::refineFUVCIS(ui depth, TaskSlot &task, bool& emptyFlag) {
    ui *idx_embedding = task.l_args->idx_embedding;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    Edges ***edge_matrix = u_args->edge_matrix;

    ui *order = u_args->order;

    std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
    std::unordered_map<VertexID, std::unordered_map<VertexID, VertexID>>& preu = u_args->preu;

    VertexID u = order[depth];

    ui** bn = u_args->bn;
    ui* bn_cnt = u_args->bn_count;

    if (FU_fns.find(u) != FU_fns.end()) {
        return;
    }

    // std::ostringstream oss;
    // oss << "bn_cnt[" << depth << "] == " << bn_cnt[depth];
    // oss << ", bn : ";
    // for (ui i = 0; i < bn_cnt[depth]; i++) {
    //     oss << bn[depth][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    for (ui i = 0; i < bn_cnt[depth]; ++i) {

        VertexID current_bn = bn[depth][i];
        // 这里要refine
        if (FU_fns.find(current_bn) != FU_fns.end()) {
            // LOG() << "refine fu vertex == " << current_bn << std::endl;

            Edges& current_edge = *edge_matrix[u][current_bn];
            ui current_index_id = idx_embedding[u];

            ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
            ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];

            ui previous_u = preu[u][current_bn];

            // LOG() << "task.l_args->FUValidCandidateIdx[current_bn][previous_u] : " << task.l_args->FUValidCandidateIdx[current_bn][previous_u] << std::endl; 

            // oss << "current_bn == " << current_bn << ", previous_u == " << previous_u << ", u == " << u << std::endl;
            // oss << "old fu's valid_candidates_count == " << task.l_args->FUValidCandidatesCount[current_bn][previous_u] << std::endl;
            // oss << "FUValidCandidateIdx : ";
            // for (ui j = 0; j < task.l_args->FUValidCandidatesCount[current_bn][previous_u]; j++) {
            //     oss << task.l_args->FUValidCandidateIdx[current_bn][previous_u][j] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();

            ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count,
                            task.l_args->FUValidCandidateIdx[current_bn][previous_u], task.l_args->FUValidCandidatesCount[current_bn][previous_u],
                            task.l_args->FUValidCandidateIdx[current_bn][u], task.l_args->FUValidCandidatesCount[current_bn][u]);
            // LOG() << "new fu's valid_candidates_count == " << task.l_args->FUValidCandidatesCount[current_bn][u] << std::endl;





            // LOG() << "task.l_args->FUValidCandidateIdx[current_bn][previous_u] : " << task.l_args->FUValidCandidateIdx[current_bn][previous_u] << std::endl; 

            // oss << "current_bn == " << current_bn << ", previous_u == " << previous_u << ", u == " << u << std::endl;
            // oss << "old fu's valid_candidates_count == " << task.l_args->FUValidCandidatesCount[current_bn][previous_u] << std::endl;
            // oss << "FUValidCandidateIdx : ";
            // for (ui j = 0; j < task.l_args->FUValidCandidatesCount[current_bn][previous_u]; j++) {
            //     oss << task.l_args->FUValidCandidateIdx[current_bn][previous_u][j] << ", ";
            // }
            // oss << std::endl;
            // LOG() << oss.str();


            // NOTE : 这里加了一个提前结束的逻辑，不然好像和原文对不上，而且效率也会降低很多
            if (task.l_args->FUValidCandidatesCount[current_bn][u] == 0) {
                // LOG() << u << " refine " << current_bn << " count == 0." << std::endl;
                emptyFlag = true;
                return;
            }
        }
    }
}

/*
    这里要改写，如果有bn是fu，那么需要用union的方式来求交
 */
void
EvaluateQuery::generateValidCandidateIndexUseUnion(ui depth, TaskSlot &task) {
    ui *idx_embedding = task.l_args->idx_embedding;
    ui *idx_count = task.l_args->idx_count;
    ui **valid_candidate_index = task.l_args->valid_candidate_idx;
    ui*& temp_buffer = task.l_args->temp_buffer;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    Edges ***edge_matrix = u_args->edge_matrix;
    ui **bn = u_args->bn;
    ui *bn_cnt = u_args->bn_count;
    ui *order = u_args->order;
    ui *u_depth = u_args->u_depth;

    std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;
    // 这个preu是要用到的
    std::unordered_map<VertexID, std::unordered_map<VertexID, VertexID>>& preu = u_args->preu;

    // std::ostringstream oss;
    // oss << "bn : ";
    // for (ui i = 0; i < bn_cnt[depth]; i++) {
    //     oss << bn[depth][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    VertexID u = order[depth];
    VertexID previous_bn = bn[depth][0];
    ui temp_count;

    ui valid_candidates_count = 0;

    // LOG() << "generate u" << u << "'s valid_candidate_index." << std::endl;

    if (FU_fns.find(previous_bn) != FU_fns.end()) {
        /*
            这个分支是遇到了冻结的点，所以要把他的valid_candidate_idx或者 FUValidCandidateIdx 的边给求并，再参与求交
        */
        // LOG() << "previsou_bn == " << previous_bn << ", is frozen" << std::endl;
        ui current_index_id;

        // 这个东西承载了求并的结果
        ui*& temp_buffer_edges = task.l_args->temp_buffer_edges;
        ui temp_buffer_edges_count;

        
        Edges& current_edge = *edge_matrix[previous_bn][u];
        ui bn_depth = u_depth[previous_bn];
        ui current_depth = u_depth[previous_bn];

        ui previous_u = preu[u][previous_bn];

        // LOG() << "log FUValidCandidatesCount " << task.l_args->FUValidCandidatesCount[previous_bn][previous_u] << std::endl;
        current_index_id = task.l_args->FUValidCandidateIdx[previous_bn][previous_u][0];
        temp_buffer_edges_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
        memcpy(temp_buffer_edges, current_edge.edge_ + current_edge.offset_[current_index_id], temp_buffer_edges_count * sizeof(ui));

        // NOTE: idx_count感觉不能动
        for (ui ii = 1; ii < task.l_args->FUValidCandidatesCount[previous_bn][previous_u]; ++ii) {
            current_index_id = task.l_args->FUValidCandidateIdx[previous_bn][previous_u][ii];

            ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
            ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];

            ComputeSetUnion::ComputeCandidates(current_candidates, current_candidates_count, temp_buffer_edges, temp_buffer_edges_count,
                                temp_buffer, temp_count);
            std::swap(temp_buffer_edges, temp_buffer);
            temp_buffer_edges_count = temp_count;
        }
        memcpy(valid_candidate_index[depth], temp_buffer_edges, temp_buffer_edges_count * sizeof(ui));
        valid_candidates_count = temp_buffer_edges_count;
        // std::ostringstream oss;
        // oss << "generate by fu backward nbr : " << previous_bn << std::endl;
        // oss << "log unioned valid_candidates_count == " << valid_candidates_count << std::endl;
        // LOG() << oss.str();
        // oss.str("");
    } else {
        // LOG() << "previsou_bn == " << previous_bn << ", not frozen" << std::endl;
        ui previous_index_id = idx_embedding[previous_bn];

        Edges& previous_edge = *edge_matrix[previous_bn][u];

        valid_candidates_count = previous_edge.offset_[previous_index_id + 1] - previous_edge.offset_[previous_index_id];
        ui* previous_candidates = previous_edge.edge_ + previous_edge.offset_[previous_index_id];

        memcpy(valid_candidate_index[depth], previous_candidates, valid_candidates_count * sizeof(ui));
    }

    
    // LOG() << "generateValidCandidateIndexUseUnion." << std::endl;
    for (ui i = 1; i < bn_cnt[depth]; ++i) {

        VertexID current_bn = bn[depth][i];
        // NOTE: 这里求并的结果不知道能不能缓存起来呢
        if (FU_fns.find(current_bn) != FU_fns.end()) {
            /*
                这个分支是遇到了冻结的点，所以要把他的valid_candidate_idx或者 FUValidCandidateIdx 的边给求并，再参与求交
            */

            ui current_index_id;

            // 这个东西承载了求并的结果
            ui*& temp_buffer_edges = task.l_args->temp_buffer_edges;
            ui temp_buffer_edges_count;

            
            Edges& current_edge = *edge_matrix[current_bn][u];
            ui bn_depth = u_depth[current_bn];
            ui current_depth = u_depth[current_bn];

            // if (preu[u][current_bn] == current_bn) {
            //     current_index_id = task.l_args->valid_candidate_idx[bn_depth][0];
            //     temp_buffer_edges_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
            //     temp_buffer_edges = current_edge.edge_ + current_edge.offset_[current_index_id];

            //     for (ui ii = 1; ii < idx_count[current_depth]; ++ii) {
            //         current_index_id = task.l_args->valid_candidate_idx[bn_depth][ii];

            //         ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
            //         ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];


            //         ComputeSetUnion::ComputeCandidates(current_candidates, current_candidates_count, temp_buffer_edges, temp_buffer_edges_count,
            //                             temp_buffer, temp_count);
            //         std::swap(temp_buffer_edges, temp_buffer);
            //         temp_buffer_edges_count = temp_count;
            //     }
            // } else {
                // 这里用 FUValidCandidateIdx[current_bn][previous_u] 代替了 valid_candidate_idx[bn_depth]
                ui previous_u = preu[u][current_bn];
                current_index_id = task.l_args->FUValidCandidateIdx[current_bn][previous_u][0];
                temp_buffer_edges_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
                memcpy(temp_buffer_edges, current_edge.edge_ + current_edge.offset_[current_index_id], temp_buffer_edges_count * sizeof(ui));
                // temp_buffer_edges = current_edge.edge_ + current_edge.offset_[current_index_id];

                // NOTE: idx_count感觉不能动
                for (ui ii = 1; ii < task.l_args->FUValidCandidatesCount[current_bn][previous_u]; ++ii) {
                    current_index_id = task.l_args->FUValidCandidateIdx[current_bn][previous_u][ii];

                    ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
                    ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];


                    ComputeSetUnion::ComputeCandidates(current_candidates, current_candidates_count, temp_buffer_edges, temp_buffer_edges_count,
                                        temp_buffer, temp_count);
                    std::swap(temp_buffer_edges, temp_buffer);
                    temp_buffer_edges_count = temp_count;
                }
            // }
            // std::ostringstream oss;
            // oss << "generate by fu backward nbr : " << current_bn << std::endl;
            // oss << "log unioned temp_buffer_edges_count == " << temp_buffer_edges_count << std::endl;

            // 求完并后再求交
            ComputeSetIntersection::ComputeCandidates(temp_buffer_edges, temp_buffer_edges_count, valid_candidate_index[depth], valid_candidates_count,
                            temp_buffer, temp_count);

            std::swap(temp_buffer, valid_candidate_index[depth]);
            valid_candidates_count = temp_count;

            // oss << "log unioned valid_candidates_count == " << valid_candidates_count << std::endl;
            // LOG() << oss.str();
            // oss.str("");
        } else {
            // 原来的求交方法，用选定的v的edges下来求交
            Edges& current_edge = *edge_matrix[current_bn][u];
            ui current_index_id = idx_embedding[current_bn];

            ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
            ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];

            // 求交走的是这个函数
            // LOG() << "valid_candidates_count = " << valid_candidates_count << ", current_candidates_count = " << current_candidates_count << std::endl;
            // TODO: 这里有valgrind警告，但是感觉不是程序问题，是求交问题
            /*
                ==3321954== Invalid read of size 32
                ==3321954==    at 0x48D0449: _mm256_loadu_si256 (avxintrin.h:929)
                ==3321954==    by 0x48D0449: ComputeSetIntersection::ComputeCNMergeBasedAVX2(unsigned int const*, unsigned int, unsigned int const*, unsigned int, unsigned int*, unsigned int&) (computesetintersection.cpp:280)
            */
            ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count, valid_candidate_index[depth], valid_candidates_count,
                            temp_buffer, temp_count);

            std::swap(temp_buffer, valid_candidate_index[depth]);
            valid_candidates_count = temp_count;
        }
    }

    idx_count[depth] = valid_candidates_count;

    if (FU_fns.find(u) != FU_fns.end()) {
        task.l_args->FUValidCandidatesCount[u][u] = valid_candidates_count;
        memcpy(task.l_args->FUValidCandidateIdx[u][u], valid_candidate_index[depth], valid_candidates_count * sizeof(ui));
    }
}

void EvaluateQuery::generateValidCandidateIndex(ui depth, TaskSlot &task) {
    ui *idx_embedding = task.l_args->idx_embedding;
    ui *idx_count = task.l_args->idx_count;
    ui **valid_candidate_index = task.l_args->valid_candidate_idx;
    ui *&temp_buffer = task.l_args->temp_buffer;
    int id_unit = task.l_args->id_unit;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    Edges ***edge_matrix = u_args->edge_matrix;
    ui **bn = u_args->bn;
    ui *bn_cnt = u_args->bn_count;
    // std::ostringstream oss;
    // oss << "bn : " << std::endl;
    // for (ui i = 0; i < bn_cnt[depth]; i++) {
    //     oss << bn[depth][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    ui *order = u_args->order;

    VertexID u = order[depth];
    VertexID previous_bn = bn[depth][0];
    ui previous_index_id = idx_embedding[previous_bn];
    ui valid_candidates_count = 0;

    Edges& previous_edge = *edge_matrix[previous_bn][u];

    valid_candidates_count = previous_edge.offset_[previous_index_id + 1] - previous_edge.offset_[previous_index_id];
    ui* previous_candidates = previous_edge.edge_ + previous_edge.offset_[previous_index_id];

    memcpy(valid_candidate_index[depth], previous_candidates, valid_candidates_count * sizeof(ui));

    ui temp_count;
    for (ui i = 1; i < bn_cnt[depth]; ++i) {
        VertexID current_bn = bn[depth][i];
        Edges& current_edge = *edge_matrix[current_bn][u];
        ui current_index_id = idx_embedding[current_bn];

        ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
        ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];

        // 求交走的是这个函数
        // LOG() << "valid_candidates_count = " << valid_candidates_count << ", current_candidates_count = " << current_candidates_count << std::endl;
        // TODO: 这里有valgrind警告，但是感觉不是程序问题，是求交问题
        /*
            ==3321954== Invalid read of size 32
            ==3321954==    at 0x48D0449: _mm256_loadu_si256 (avxintrin.h:929)
            ==3321954==    by 0x48D0449: ComputeSetIntersection::ComputeCNMergeBasedAVX2(unsigned int const*, unsigned int, unsigned int const*, unsigned int, unsigned int*, unsigned int&) (computesetintersection.cpp:280)
        */
        ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count, valid_candidate_index[depth], valid_candidates_count,
                        temp_buffer, temp_count);

        std::swap(temp_buffer, valid_candidate_index[depth]);
        valid_candidates_count = temp_count;
    }

    idx_count[depth] = valid_candidates_count;
}

// void EvaluateQuery::updateFrozenCandidateIndex(ui depth, TaskSlot &task, ui* u_depth,
//     std::unordered_map<VertexID, std::unordered_map<VertexID, ui*>> FUValidCandidateIdx,
//     std::unordered_map<VertexID, std::unordered_map<VertexID, ui>> FUValidCandidatesCount) {
//     ui *idx_embedding = task.l_args->idx_embedding;
//     ui *&temp_buffer = task.l_args->temp_buffer;
//     int id_unit = task.l_args->id_unit;
//     UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];

//     Edges ***edge_matrix = u_args->edge_matrix;

//     ui *order = u_args->order;
//     std::unordered_map<VertexID, std::vector<VertexID>>& FU_fns = u_args->FU_fns;

//     VertexID u = order[depth];

//     /*
//         修改这个 u 的 backward_frozen_u 的 FUValidCandidateIdx
//     */
//     ui** bn = u_args->bn;
//     ui* bn_count = u_args->bn_count;
    
//     std::ostringstream oss;
//     oss << "bn of u" << u << " : " << std::endl;
//     for (ui i = 0; i < bn_count[u]; i++) {
//         oss << bn[u][i] << ", ";
//     }
//     oss << std::endl;
//     LOG() << oss.str();
//     oss.str("");

//     /*
//         NOTE: 这里默认 backward_frozen_u 的 FUValidCandidateIdx 已经在到了那一层的时候初始化为 valid_candidate_idx 了
//         NOTE: 对于每一个frozen_u，都要维护一个fn的有序队列
//         这个有序队列实际上就是一个FU_fns的那个vector了，所以在构造的时候要排好序
//     */
//     for (ui i = 0; i < bn_count[u]; i++) {
//         if (FU_fns.find(bn[u][i]) == FU_fns.end()) {
//             continue;
//         }
//         VertexID frozen_u = bn[u][i];
//         VertexID preu;
//         for (auto iter = FU_fns[frozen_u].begin(); iter != FU_fns[frozen_u].end(); iter++) {
//             if (*iter == u) {
//                 if (iter == FU_fns[frozen_u].begin()) {
//                     preu = *iter;
//                 } else {
//                     iter--;
//                     preu = *iter;
//                 }
//                 break;
//             }
//         }
//         ui previous_candidates_count;
//         ui* previous_candidates;
//         if (preu == u) {
//             previous_candidates = task.l_args->valid_candidate_idx[u_depth[frozen_u]];
//             previous_candidates_count = task.l_args->idx_count[u_depth[frozen_u]];
//         } else {
//             previous_candidates = FUValidCandidateIdx[frozen_u][preu];
//             previous_candidates_count = FUValidCandidatesCount[frozen_u][preu];
//         }

//         Edges& current_edge = *edge_matrix[u][frozen_u];
//         ui current_index_id = idx_embedding[u];
//         ui temp_count;
//         ui current_candidates_count = current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
//         ui* current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];
//         ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count, previous_candidates, previous_candidates_count,
//                         FUValidCandidateIdx[frozen_u][u], FUValidCandidatesCount[frozen_u][u]);
//     }
// }

void EvaluateQuery::generateValidCandidates(const Graph *data_graph, ui depth, TaskSlot& task) {
    ui* embedding = task.l_args->embedding;
    ui* idx_count = task.l_args->idx_count;
    bool* visited_vertices = task.l_args->visited_vertices;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[task.l_args->id_unit];
    ui** bn = u_args->bn;
    ui* bn_cnt = u_args->bn_count;
    ui* order = u_args->order;
    ui** candidates = u_args->candidates;
    ui* candidates_count = u_args->candidates_count;
    ui** valid_candidate = task.l_args->valid_candidate;

    VertexID u = order[depth];

    idx_count[depth] = 0;
    // LOG() << "generating valid candidates for depth = " << depth << std::endl;
    for (ui i = 0; i < candidates_count[u]; ++i) {
        VertexID v = candidates[u][i];

        if (!visited_vertices[v]) {
            bool valid = true;

            for (ui j = 0; j < bn_cnt[depth]; ++j) {
                VertexID u_nbr = bn[depth][j];
                VertexID u_nbr_v = embedding[u_nbr];

                if (!data_graph->checkEdgeExistence(v, u_nbr_v)) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                valid_candidate[depth][idx_count[depth]++] = v;
            }
        }
    }
    // LOG() << "over generating valid candidates for depth = " << depth << std::endl;
}

void EvaluateQuery::generateValidCandidates(const Graph *query_graph, const Graph *data_graph, ui depth, ui *embedding,
                                            ui *idx_count, ui **valid_candidate, bool *visited_vertices, ui **bn,
                                            ui *bn_cnt,
                                            ui *order, ui *pivot) {
    VertexID u = order[depth];
    LabelID u_label = query_graph->getVertexLabel(u);
    ui u_degree = query_graph->getVertexDegree(u);

    idx_count[depth] = 0;

    VertexID p = embedding[pivot[depth]];
    ui nbr_cnt;
    const VertexID *nbrs = data_graph->getVertexNeighbors(p, nbr_cnt);

    for (ui i = 0; i < nbr_cnt; ++i) {
        VertexID v = nbrs[i];

        if (!visited_vertices[v] && u_label == data_graph->getVertexLabel(v) &&
            u_degree <= data_graph->getVertexDegree(v)) {
            bool valid = true;

            for (ui j = 0; j < bn_cnt[depth]; ++j) {
                VertexID u_nbr = bn[depth][j];
                VertexID u_nbr_v = embedding[u_nbr];

                if (!data_graph->checkEdgeExistence(v, u_nbr_v)) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                valid_candidate[depth][idx_count[depth]++] = v;
            }
        }
    }
}

void
EvaluateQuery::updateExtendableVertex(VertexID mapped_vertex, TaskSlot& task, UnitArgs* u_args) {

    ui *idx_embedding = task.l_args->idx_embedding;
    ui *u_idx_count = task.l_args->u_idx_count;
    ui **valid_candidate_index = task.l_args->valid_candidate_idx;
    ui *&temp_buffer = task.l_args->temp_buffer;
    ui *extendable = task.l_args->extendable;
    std::vector<dpiso_min_pq> &vec_rank_queue = task.l_args->vec_rank_queue;

    Edges ***edge_matrix = u_args->edge_matrix;
    ui **weight_array = u_args->weight_array;
    TreeNode *tree = u_args->tree;
    Graph *query_graph = u_args->query_graph->subgraph;
    
    TreeNode &node = tree[mapped_vertex];
    for (ui i = 0; i < node.fn_count_; ++i) {
        VertexID u = node.fn_[i];
        extendable[u] -= 1;
        if (extendable[u] == 0) {
            // if (u == 6) {
            //     LOG() << "u == 6,  tree[u].bn_count_ == " << tree[u].bn_count_ << std::endl;
            // }
            // generateValidCandidateIndex(cur_depth, task);
            generateValidCandidateIndex(u, idx_embedding, u_idx_count, valid_candidate_index[u], edge_matrix, tree[u].bn_,
                                        tree[u].bn_count_, temp_buffer);

            ui weight = 0;
            if (u_idx_count[u] == 0) {
                // LOG() << "Error: u_idx_count == 0, u == " << u << std::endl;
            }
            // if (u == 6) {
            //     LOG() << "u_idx_count[" << u << "] == " << u_idx_count[u] << std::endl;
            // }
            for (ui j = 0; j < u_idx_count[u]; ++j) {
                ui idx = valid_candidate_index[u][j];
                weight += weight_array[u][idx];
            }
            // LOG() << "u" << u << "'s calculated extendable[u] == " << extendable[u] << ", weight = " << weight << std::endl;
            if (vec_rank_queue.size() == 0) { 
                LOG() << "Error: vec_rank_queue is empty." << std::endl;
                abort();
            }
            vec_rank_queue.back().emplace(std::make_pair(std::make_pair(u, query_graph->getVertexDegree(u)), weight));
        } else {
            // LOG() << "extendable[" << u << "] = " << extendable[u] << std::endl;
        }
    }
}

// void EvaluateQuery::updateExtendableVertex(ui *idx_embedding, ui *idx_count, ui **valid_candidate_index,
//                                            Edges ***edge_matrix, ui *&temp_buffer, ui **weight_array,
//                                            TreeNode *tree, VertexID mapped_vertex, ui *extendable,
//                                            std::vector<dpiso_min_pq> &vec_rank_queue, const Graph *query_graph) {
//     TreeNode &node = tree[mapped_vertex];
//     for (ui i = 0; i < node.fn_count_; ++i) {
//         VertexID u = node.fn_[i];
//         extendable[u] -= 1;
//         if (extendable[u] == 0) {
//             generateValidCandidateIndex(u, idx_embedding, idx_count, valid_candidate_index[u], edge_matrix, tree[u].bn_,
//                                         tree[u].bn_count_, temp_buffer);

//             ui weight = 0;
//             for (ui j = 0; j < idx_count[u]; ++j) {
//                 ui idx = valid_candidate_index[u][j];
//                 weight += weight_array[u][idx];
//             }
//             vec_rank_queue.back().emplace(std::make_pair(std::make_pair(u, query_graph->getVertexDegree(u)), weight));
//         }
//     }
// }

void EvaluateQuery::restoreExtendableVertex(TreeNode *tree, VertexID unmapped_vertex, ui *extendable) {
    TreeNode &node = tree[unmapped_vertex];
    for (ui i = 0; i < node.fn_count_; ++i) {
        VertexID u = node.fn_[i];
        extendable[u] += 1;
    }
}

void
EvaluateQuery::generateValidCandidateIndex(VertexID u, ui *idx_embedding, ui *u_idx_count, ui *&valid_candidate_index,
                                           Edges ***edge_matrix, ui *bn, ui bn_cnt, ui *&temp_buffer) {
    VertexID previous_bn = bn[0];
    Edges &previous_edge = *edge_matrix[previous_bn][u];
    ui previous_index_id = idx_embedding[previous_bn];

    ui previous_candidates_count =
            previous_edge.offset_[previous_index_id + 1] - previous_edge.offset_[previous_index_id];
    ui *previous_candidates = previous_edge.edge_ + previous_edge.offset_[previous_index_id];

    ui valid_candidates_count = 0;
    for (ui i = 0; i < previous_candidates_count; ++i) {
        valid_candidate_index[valid_candidates_count++] = previous_candidates[i];
    }
    // if (u == 13) {
    //     LOG() << "generate u == " << u << ", current_bn == " << previous_bn << ", edge size = " << valid_candidates_count << std::endl;
    //     LOG() << "edge size : " << valid_candidates_count << std::endl;
    // }

    ui temp_count;
    for (ui i = 1; i < bn_cnt; ++i) {
        VertexID current_bn = bn[i];
        Edges &current_edge = *edge_matrix[current_bn][u];
        ui current_index_id = idx_embedding[current_bn];

        ui current_candidates_count =
                current_edge.offset_[current_index_id + 1] - current_edge.offset_[current_index_id];
        ui *current_candidates = current_edge.edge_ + current_edge.offset_[current_index_id];

        // if (u == 13) {
        //     LOG() << "generate u == " << u << ", current_bn == " << current_bn << ", edge size = " << current_candidates_count << std::endl;
        //     LOG() << "edge : " << current_candidates[0] << std::endl;
        // }

        ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count, valid_candidate_index,
                                                  valid_candidates_count,
                                                  temp_buffer, temp_count);

        std::swap(temp_buffer, valid_candidate_index);
        valid_candidates_count = temp_count;
    }
    u_idx_count[u] = valid_candidates_count;
    // if (u == 13) {
    //     std::ostringstream oss;
    //     oss << "u" << u << "'s generated u_idx_count " << u_idx_count[u]  << std::endl;
    //     oss << "u13 valid_candidate_index : ";
    //     for (ui i = 0; i < u_idx_count[u]; i++) {
    //         oss << valid_candidate_index[i] << ", ";
    //     }
    //     oss << std::endl;
    //     LOG() << oss.str();
    // }
}

void EvaluateQuery::generateValidCandidateIndexWithPivot(const Graph* data_graph, ui depth, TaskSlot &task) {
    ui *idx_embedding = task.l_args->idx_embedding;
    ui *idx_count = task.l_args->idx_count;
    ui **valid_candidate_index = task.l_args->valid_candidate_idx;
    bool *visited_vertices = task.l_args->visited_vertices;
    int id_unit = task.l_args->id_unit;
    ui *embedding = task.l_args->embedding;

    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    Edges ***edge_matrix = u_args->edge_matrix;
    ui **bn = u_args->bn;
    ui *bn_cnt = u_args->bn_count;
    ui *order = u_args->order;
    ui *pivot = u_args->pivots;
    ui **candidates = u_args->candidates;

    VertexID u = order[depth];
    VertexID pivot_vertex = pivot[depth];
    ui idx_id = idx_embedding[pivot_vertex];
    Edges &edge = *edge_matrix[pivot_vertex][u];
    ui count = edge.offset_[idx_id + 1] - edge.offset_[idx_id];
    ui *candidate_idx = edge.edge_ + edge.offset_[idx_id];

    ui valid_candidate_index_count = 0;

    if (bn_cnt[depth] == 0) {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v])
                valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
        }
    } else {
        for (ui i = 0; i < count; ++i) {
            ui temp_idx = candidate_idx[i];
            VertexID temp_v = candidates[u][temp_idx];

            if (!visited_vertices[temp_v]) {
                bool valid = true;

                for (ui j = 0; j < bn_cnt[depth]; ++j) {
                    VertexID u_bn = bn[depth][j];
                    VertexID u_bn_v = embedding[u_bn];

                    if (!data_graph->checkEdgeExistence(temp_v, u_bn_v)) {
                        valid = false;
                        break;
                    }
                }

                if (valid)
                    valid_candidate_index[depth][valid_candidate_index_count++] = temp_idx;
            }
        }
    }
    idx_count[depth] = valid_candidate_index_count;
}

void EvaluateQuery::computeAncestor(const Graph *query_graph, TreeNode *tree, VertexID *order,
                                    std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors) {
    ui query_vertices_num = query_graph->getVerticesCount();
    ancestors.resize(query_vertices_num);

    // Compute the ancestor in the top-down order.
    for (ui i = 0; i < query_vertices_num; ++i) {
        VertexID u = order[i];
        TreeNode &u_node = tree[u];
        ancestors[u].set(u);
        for (ui j = 0; j < u_node.bn_count_; ++j) {
            VertexID u_bn = u_node.bn_[j];
            ancestors[u] |= ancestors[u_bn];
        }
    }
}

void EvaluateQuery::generateValidCandidates(ui depth, TaskSlot& task) {
    ui *embedding = task.l_args->embedding;
    ui **valid_candidates = task.l_args->valid_candidate;
    ui *idx_count = task.l_args->idx_count;
    ui *&temp_buffer = task.l_args->temp_buffer;
    int id_unit = task.l_args->id_unit;
    UnitArgs* u_args = TaskSlot::g_args->unitArgsVec[id_unit];
    ui query_vertices_count = u_args->query_graph->getVerticesCount();
    TreeNode *tree = u_args->tree;
    std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>& TE_Candidates = TaskSlot::g_args->TE_Candidates;
    std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>>& NTE_Candidates = TaskSlot::g_args->NTE_Candidates;
    ui *order = u_args->order;

    VertexID u = order[depth];
    idx_count[depth] = 0;
    ui valid_candidates_count = 0;
    ui old_u = u_args->query_graph->new2olds[u];
    {
        VertexID u_p = tree[u].parent_;
        VertexID v_p = embedding[u_p];

        // auto iter = TE_Candidates[old_u].find(v_p);
        // if (iter == TE_Candidates[old_u].end() || iter->second.empty()) {
        auto iter = TE_Candidates[u].find(v_p);
        if (iter == TE_Candidates[u].end() || iter->second.empty()) {

std::ostringstream oss;

// oss << "Candidates: " << std::endl;
// for (ui i = 0; i < query_vertices_count; ++i) {
//     VertexID u = i;
//     oss << u << ':';
//     for (ui j = 0; j < candidates_count[u]; ++j) {
//         oss << candidates[u][j] << ' ';
//     }
//     oss << std::endl;
// }


//             LOG() << "old_u == " << old_u << ", u_p == " << u_p << ", v_p == " << v_p << std::endl;


// oss << "log TE_Candidates: " << std::endl;
// for (ui i = 1; i < query_vertices_count; ++i) {
//     VertexID u = order[i];
//     // TE_Candidates
//     oss << "TE_Candidates: " << u << ',' << tree[u].parent_ << std::endl;
//     for (auto iter = TE_Candidates[u].begin(); iter != TE_Candidates[u].end(); ++iter) {
//         oss << iter->first << ": ";
//         for (auto v : iter->second) {
//             oss << v << ' ';
//             // if (!data_graph->checkEdgeExistence(iter->first, v)) {
//             //     oss << "Edge does not exist" << std::endl;
//             // }
//         }
//         oss << std::endl;
//     }
//     oss << "-----" << std::endl;
//     for (ui j = 0; j < tree[u].bn_count_; ++j) {
//         VertexID u_bn = tree[u].bn_[j];
//         oss << "NTE_Candidates: " << u << ',' << u_bn << std::endl;
//         for (auto iter = NTE_Candidates[u][u_bn].begin(); iter != NTE_Candidates[u][u_bn].end(); ++iter) {
//             oss << iter->first << ": ";
//             for (auto v : iter->second) {
//                 oss << v << ' ';
//                 // if (!data_graph->checkEdgeExistence(iter->first, v)) {
//                 //     oss << "Edge does not exist" << std::endl;
//                 // }
//             }
//             oss << std::endl;
//         }
//         oss << "-----" << std::endl;
//     }
// }

// LOG() << oss.str();
// oss.str("");

            // LOG() << "TE_Candidates is empty." << std::endl;
            // if (iter == TE_Candidates[old_u].end()) {
            //     LOG() << "TE_Candidates[old_u] is empty." << std::endl;
            // } else if (iter->second.empty()) {
            //     LOG() << "iter->second is empty." << std::endl;
            // }
            return;
        }

        valid_candidates_count = iter->second.size();
        VertexID *v_p_nbrs = iter->second.data();

        for (ui i = 0; i < valid_candidates_count; ++i) {
            valid_candidates[depth][i] = v_p_nbrs[i];
        }
    }
    ui temp_count;
    for (ui i = 0; i < tree[u].bn_count_; ++i) {
        VertexID u_p = tree[u].bn_[i];
        VertexID v_p = embedding[u_p];
        ui old_u_p = u_args->query_graph->new2olds[u_p];

        // auto iter = NTE_Candidates[old_u][old_u_p].find(v_p);
        // if (iter == NTE_Candidates[old_u][old_u_p].end() || iter->second.empty()) {
        auto iter = NTE_Candidates[u][u_p].find(v_p);
        if (iter == NTE_Candidates[u][u_p].end() || iter->second.empty()) {
            // LOG() << "NTE_Candidates is empty." << std::endl;
            return;
        }

        ui current_candidates_count = iter->second.size();
        ui *current_candidates = iter->second.data();

        ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count,
                                                  valid_candidates[depth], valid_candidates_count,
                                                  temp_buffer, temp_count);

        std::swap(temp_buffer, valid_candidates[depth]);
        valid_candidates_count = temp_count;
    }

    idx_count[depth] = valid_candidates_count;
    // std::ostringstream oss;
    // oss << "depth == " << depth << ", u == " << u << ", log valid_candidates_count == " << valid_candidates_count << std::endl;
    // oss << "valid_candidates : " << std::endl;
    // for (ui i = 0; i < valid_candidates_count; i++) {
    //     oss << valid_candidates[depth][i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
}

void EvaluateQuery::generateValidCandidates(ui depth, ui *embedding, ui *idx_count, ui **valid_candidates, ui *order,
                                            ui *&temp_buffer, TreeNode *tree,
                                            std::vector<std::unordered_map<VertexID, std::vector<VertexID>>> &TE_Candidates,
                                            std::vector<std::vector<std::unordered_map<VertexID, std::vector<VertexID>>>> &NTE_Candidates) {

    VertexID u = order[depth];
    idx_count[depth] = 0;
    ui valid_candidates_count = 0;
    {
        VertexID u_p = tree[u].parent_;
        VertexID v_p = embedding[u_p];

        auto iter = TE_Candidates[u].find(v_p);
        if (iter == TE_Candidates[u].end() || iter->second.empty()) {
            return;
        }

        valid_candidates_count = iter->second.size();
        VertexID *v_p_nbrs = iter->second.data();

        for (ui i = 0; i < valid_candidates_count; ++i) {
            valid_candidates[depth][i] = v_p_nbrs[i];
        }
    }
    ui temp_count;
    for (ui i = 0; i < tree[u].bn_count_; ++i) {
        VertexID u_p = tree[u].bn_[i];
        VertexID v_p = embedding[u_p];

        auto iter = NTE_Candidates[u][u_p].find(v_p);
        if (iter == NTE_Candidates[u][u_p].end() || iter->second.empty()) {
            return;
        }

        ui current_candidates_count = iter->second.size();
        ui *current_candidates = iter->second.data();

        ComputeSetIntersection::ComputeCandidates(current_candidates, current_candidates_count,
                                                  valid_candidates[depth], valid_candidates_count,
                                                  temp_buffer, temp_count);

        std::swap(temp_buffer, valid_candidates[depth]);
        valid_candidates_count = temp_count;
    }

    idx_count[depth] = valid_candidates_count;
}

void EvaluateQuery::computeAncestor(const Graph *query_graph, ui **bn, ui *bn_cnt, VertexID *order,
                                    std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors) {
    ui query_vertices_num = query_graph->getVerticesCount();
    ancestors.resize(query_vertices_num);

    // Compute the ancestor in the top-down order.
    for (ui i = 0; i < query_vertices_num; ++i) {
        VertexID u = order[i];
        ancestors[u].set(u);
        for (ui j = 0; j < bn_cnt[i]; ++j) {
            VertexID u_bn = bn[i][j];
            ancestors[u] |= ancestors[u_bn];
        }
    }
}

void EvaluateQuery::computeAncestor(const Graph *query_graph, VertexID *order,
                                    std::vector<std::bitset<MAXIMUM_QUERY_GRAPH_SIZE>> &ancestors) {
    ui query_vertices_num = query_graph->getVerticesCount();
    ancestors.resize(query_vertices_num);

    // Compute the ancestor in the top-down order.
    for (ui i = 0; i < query_vertices_num; ++i) {
        VertexID u = order[i];
        ancestors[u].set(u);
        for (ui j = 0; j < i; ++j) {
            VertexID u_bn = order[j];
            if (query_graph->checkEdgeExistence(u, u_bn)) {
                ancestors[u] |= ancestors[u_bn];
            }
        }
    }
}


