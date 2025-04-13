#include <cassert>
#include <pthread.h>

#include "hashjoin.h"
#include "hashTable.h"
#include "keyTraits.h"
#include "utility/statistics/Logs.h"
#include "utility/statistics/intTimer.h"
#include "matching/Args.h"

std::unique_ptr<JoinKey>
NoPartitionHashJoin::create_join_key(size_t key_size) {
    if (key_size == 1) {
        // LOG() << "key_size == 1" << std::endl;
        return std::make_unique<FixedSizeJoinKey<uint32_t>>();
    } else if (key_size == 2) {
        // LOG() << "key_size == 2" << std::endl;
        return std::make_unique<FixedSizeJoinKey<uint64_t>>();
    } else if (key_size == 3 || key_size == 4) {
        // LOG() << "key_size == 3|4" << std::endl;
        return std::make_unique<FixedSizeJoinKey<__uint128_t>>();
    } else if (key_size <= 8) {
        // LOG() << "key_size <= 8" << std::endl;
        return std::make_unique<ArrayJoinKey>();
    } else {
        // LOG() << "key_size > 8" << std::endl;
        return std::make_unique<VectorJoinKey>();
    }
}

NoPartitionHashJoin::NoPartitionHashJoin(ui data_graph_vcnt, PartialMatch* Rori, PartialMatch* Sori,
    std::unordered_set<ui>& RjoinIdxSet, std::unordered_set<ui>& SjoinIdxSet,
    ui new_length, int64_t time_limit, ui d_graph_vcnt,
    bool* visited, ui* visited_flag, ui thread_num, int joinround)
    : data_graph_vcnt(data_graph_vcnt), time_limit(time_limit), thread_num(thread_num), joinround(joinround) {
    // LOG() << "time_limit == " << time_limit << std::endl;
    /*
        NOTE: 这里是需要重新排序的,否则像下面这种就会出问题
            join RjoinIdxs corresponding uids: 5, 2, 3, 17, 
            join SjoinIdxs corresponding uids: 2, 5, 3, 17, 
    */
    std::vector<std::pair<ui, ui>> RjoinIdx2Uid;
    std::vector<std::pair<ui, ui>> SjoinIdx2Uid;
    for (auto& idx : RjoinIdxSet) {
        RjoinIdx2Uid.push_back(std::make_pair(Rori->new2olds[idx], idx));
    }
    for (auto& idx : SjoinIdxSet) {
        SjoinIdx2Uid.push_back(std::make_pair(Sori->new2olds[idx], idx));
    }
    std::sort(RjoinIdx2Uid.begin(), RjoinIdx2Uid.end());
    std::sort(SjoinIdx2Uid.begin(), SjoinIdx2Uid.end());

    for (auto& [_, idx] : RjoinIdx2Uid) {
        this->RjoinIdxs.push_back(idx);
    }
    for (auto& [_, idx] : SjoinIdx2Uid) {
        this->SjoinIdxs.push_back(idx);
    }
    // 小的放左边，大的放右边
    if (Rori->size > Sori->size) {
        // LOG() << "swap the two relation" << std::endl;
        this->R = Sori;
        this->S = Rori;
        std::swap(RjoinIdxs, SjoinIdxs);
        std::swap(RjoinIdxSet, SjoinIdxSet);
    } else {
        this->R = Rori;
        this->S = Sori;
    }
    this->RjoinIdxSet = RjoinIdxSet;
    this->SjoinIdxSet = SjoinIdxSet;

    // LOG() << "RjoinIdxSet : " << this->RjoinIdxSet.size() << ", SjoinIdxSet : " << this->RjoinIdxSet.size() << std::endl;

    // LOG() << "---------In Join : " << std::endl;

    // std::ostringstream oss;
    // oss << "RjoinIdxs : ";
    // for (auto& idx : RjoinIdxSet) {
    //     oss << idx << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    // oss << "SjoinIdxs : ";
    // for (auto& idx : SjoinIdxSet) {
    //     oss << idx << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // oss << "R's old uids : ";
    // for (ui i = 0; i < R->length; ++i) {
    //     oss << R->new2olds[i] << ", ";
    // }
    // oss << std::endl;
    // oss << "S's old uids : ";
    // for (ui i = 0; i < S->length; ++i) {
    //     oss << S->new2olds[i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();

    // LOG() << "R.size == " << R->size << ", S.size == " << S->size << std::endl;



    // S直接拷贝，R需要重新构建
    joined = new PartialMatch(new_length, R->total_vertices_count, R->size + S->size);
    
    std::copy(S->new2olds, S->new2olds + S->length, joined->new2olds);
    std::copy(S->old2news, S->old2news + S->total_vertices_count, joined->old2news);
    ui now_length = S->length;
    for (ui i = 0; i < R->length; ++i) {
        if (RjoinIdxSet.find(i) != RjoinIdxSet.end()) {
            continue;
        }
        // LOG() << "add R's idx " << i << ", old_id == " << R->new2olds[i] << std::endl;
        joined->new2olds[now_length] = R->new2olds[i];
        joined->old2news[R->new2olds[i]] = now_length;
        now_length++;
    }
    // LOG() << "new_length == " << new_length << ", now_length == " << now_length << std::endl;
    assert(new_length == now_length);


    // oss.str("");
    // for (ui i = 0; i < joined->length; ++i) {
    //     oss << "new2olds[" << i << "] == " << joined->new2olds[i] << std::endl;
    // }
    // for (ui i = 0; i < joined->total_vertices_count; ++i) {
    //     oss << "old2news[" << i << "] == " << joined->old2news[i] << std::endl;
    // }
    // LOG() << oss.str();
    if (thread_num > 1) {
        for (int ii = 0; ii < thread_num; ++ii) {
            // S直接拷贝，R需要重新构建
            PartialMatch* local_joined = new PartialMatch(new_length, R->total_vertices_count, R->size + S->size);
            
            std::copy(S->new2olds, S->new2olds + S->length, local_joined->new2olds);
            std::copy(S->old2news, S->old2news + S->total_vertices_count, local_joined->old2news);
            ui now_length = S->length;
            for (ui i = 0; i < R->length; ++i) {
                if (RjoinIdxSet.find(i) != RjoinIdxSet.end()) {
                    continue;
                }
                // LOG() << "add R's idx " << i << ", old_id == " << R->new2olds[i] << std::endl;
                local_joined->new2olds[now_length] = R->new2olds[i];
                local_joined->old2news[R->new2olds[i]] = now_length;
                now_length++;
            }
            // LOG() << "new_length == " << new_length << ", now_length == " << now_length << std::endl;
            assert(new_length == now_length);

            // oss.str("");
            // for (ui i = 0; i < local_joined->length; ++i) {
            //     oss << "new2olds[" << i << "] == " << local_joined->new2olds[i] << std::endl;
            // }
            // for (ui i = 0; i < local_joined->total_vertices_count; ++i) {
            //     oss << "old2news[" << i << "] == " << local_joined->old2news[i] << std::endl;
            // }
            // LOG() << oss.str();

            local_joined_s.push_back(local_joined);
        }

        // local_visited_s.resize(thread_num);
        // local_visited_flag_s.resize(thread_num);
        // local_visited_cnt_s.resize(thread_num);
        // for (int ii = 0; ii < thread_num; ++ii) {
        //     local_visited_s[ii] = new bool[data_graph_vcnt]();
        //     local_visited_flag_s[ii] = new ui[data_graph_vcnt]();
        //     local_visited_cnt_s[ii] = 0;
        // }
    }
}

// PartialMatch*
// NoPartitionHashJoin::execute() {
//     HashTable hash_table(1024);
//     std::ostringstream oss;

//     // auto start = std::chrono::steady_clock::now();
//     // LOG() << "output R: " << std::endl;
//     // R->logAll();
//     // LOG() << "output S: " << std::endl;
//     // S->logAll();

//     // oss << "join original uids: ";
//     // for (auto& u : SjoinIdxs) {
//     //     oss << S->new2olds[u] << ", ";
//     // }
//     // oss << std::endl;
//     // oss << "join RjoinIdxs: ";
//     // for (auto& u : RjoinIdxs) {
//     //     oss << R->new2olds[u] << ", ";
//     // }
//     // oss << std::endl;
//     // oss << "join SjoinIdxs: ";
//     // for (auto& u : SjoinIdxs) {
//     //     oss << S->new2olds[u] << ", ";
//     // }
//     // oss << std::endl;
//     // LOG() << oss.str();
//     // oss.str("");
//     // 构建阶段
//     for (uint32_t i = 0; i < R->getSize(); ++i) {
//         uint32_t* data = R->getTuple(i);
        
//         // 动态创建 JoinKey
//         std::unique_ptr<JoinKey> key = create_join_key(RjoinIdxs.size());
//         key->construct(data, RjoinIdxs);

//         // if (i == 48) {
//         //     oss << "log 48-th R data: ";
//         //     for (ui j = 0; j < R->length; ++j) {
//         //         oss << data[j] << ", ";
//         //     }
//         //     oss << std::endl;
//         //     oss << "48-th key == " << key->toString() << std::endl;
//         //     LOG() << oss.str();
//         //     oss.str("");
//         // }

//         if (hash_table.find(key) == hash_table.end()) {
//             // NOTE : 预分配64大小
//             std::vector<ui> temp;
//             temp.push_back(i);
//             hash_table[std::move(key)] = std::move(temp);
//         } else {
//             hash_table[std::move(key)].push_back(i);
//         }
//     }

//     if (checkOverTime()) {
//         return handleResp();
//     }
//     // auto end = std::chrono::steady_clock::now();
//     // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
//     // LOG() << "build_hashtable_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;
//     // log the hash_table
//     // oss << "log the hash_table: ";
//     // for (auto& [k, v] : hash_table) {
//     //     oss << k->toString() << " -> ";
//     //     for (auto& idx : v) {
//     //         oss << idx << ", ";
//     //     }
//     //     oss << std::endl;
//     // }
//     // LOG() << oss.str();
//     // oss.str("");


//     // LOG() << "hashtable bucket size == " << hash_table.bucket_count() << std::endl;
//     // LOG() << "hashtable size == " << hash_table.size() << std::endl;
//     // printHashTable(hash_table);

//     // 探测阶段
//     // auto findstart = std::chrono::steady_clock::now();
//     // auto findend = std::chrono::steady_clock::now();

//     // auto constrcutstart = std::chrono::steady_clock::now();
//     // auto constrcutend = std::chrono::steady_clock::now();

//     // start = std::chrono::steady_clock::now();
    
//     // 探测阶段：并行探测
//     auto start = std::chrono::steady_clock::now();

//     for (uint64_t i = 0; i < S->getSize(); ++i) {
//         // findstart = std::chrono::steady_clock::now();
//         if (checkOverTime()) {
//             // LOG() << "processing " << i << " of " << S->getSize() << std::endl;
//             // LOG() << "overtime, exit" << std::endl;
//             return handleResp();
//         }
//         // S->logSingleRow(i);
//         uint32_t* data = S->getTuple(i);
//         std::unique_ptr<JoinKey> key = create_join_key(SjoinIdxs.size());
//         key->construct(data, SjoinIdxs);
//         auto iter = hash_table.find(key);

//         // if (i == 95) {
//         //     oss << "log 95-th S data: ";
//         //     for (ui j = 0; j < S->length; ++j) {
//         //         oss << data[j] << ", ";
//         //     }
//         //     oss << std::endl;
//         //     oss << "95-th key == " << key->toString() << std::endl;
//         //     LOG() << oss.str();
//         //     oss.str("");
//         // }

//         // findend = std::chrono::steady_clock::now();
//         // find_key_time += std::chrono::duration_cast<std::chrono::nanoseconds>(findend - findstart).count();

//         // constrcutstart = std::chrono::steady_clock::now();
//         if (iter != hash_table.end()) {
//             if (checkOverTime()) {
//                 // LOG() << "processing " << i << " of " << S->getSize() << std::endl;
//                 // LOG() << "overtime, exit" << std::endl;
//                 return handleResp();
//             }
//             // LOG() << key->toString() << " found, idxs.size == " << iter->second.size() << std::endl;
//             std::vector<uint32_t>& row_indices = iter->second;
//             for (auto& row_index : row_indices) {
//                 // LOG() << "R row == " << row_index << ", S row == " << i << std::endl;
//                 // auto exactstart = std::chrono::steady_clock::now();
//                 constructRow(row_index, i);
//                 // auto exactend = std::chrono::steady_clock::now();
//                 // exact_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(exactend - exactstart).count();
//             }
//         } else {
//             // LOG() << key->toString() << ", not found." << std::endl;
//         }
//         // constrcutend = std::chrono::steady_clock::now();
//         // construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(constrcutend - constrcutstart).count();
//     }

//     // auto thread_end = std::chrono::steady_clock::now();
//     // double thread_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(thread_end - thread_start).count();
//     // oss.str("");
//     // oss << "joinround: " << joinround << ", ";
//     // oss << "thread_id == " << 0 << ", ";
//     // oss << "thread_hashjoin_time_in_ns == " << intTimer::TEMPNANOSECTOSEC(thread_hashjoin_time_in_ns);
//     // oss << ", thread_round_result.size == " << joined->size << std::endl;
//     // LOG() << oss.str();

//     auto end = std::chrono::steady_clock::now();
//     auto single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
//     LOG() << "probe_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;

//     return joined;
// }

PartialMatch*
NoPartitionHashJoin::execute() {
    omp_set_num_threads(thread_num);
    // LOG() << "thread_num == " << thread_num << std::endl;
    // auto thread_start = std::chrono::steady_clock::now();
    HashTable hash_table(1024);
    std::ostringstream oss;

    // auto start = std::chrono::steady_clock::now();
    // LOG() << "output R: " << std::endl;
    // R->logAll();
    // LOG() << "output S: " << std::endl;
    // S->logAll();

    // oss << "join original uids: ";
    // for (auto& u : SjoinIdxs) {
    //     oss << S->new2olds[u] << ", ";
    // }
    // oss << std::endl;
    // oss << "join RjoinIdxs: ";
    // for (auto& u : RjoinIdxs) {
    //     oss << R->new2olds[u] << ", ";
    // }
    // oss << std::endl;
    // oss << "join SjoinIdxs: ";
    // for (auto& u : SjoinIdxs) {
    //     oss << S->new2olds[u] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    // 构建阶段
    #pragma omp parallel for
    for (uint32_t i = 0; i < R->getSize(); ++i) {
        uint32_t* data = R->getTuple(i);
        
        // 动态创建 JoinKey
        std::unique_ptr<JoinKey> key = create_join_key(RjoinIdxs.size());
        key->construct(data, RjoinIdxs);

        // if (i == 48) {
        //     oss << "log 48-th R data: ";
        //     for (ui j = 0; j < R->length; ++j) {
        //         oss << data[j] << ", ";
        //     }
        //     oss << std::endl;
        //     oss << "48-th key == " << key->toString() << std::endl;
        //     LOG() << oss.str();
        //     oss.str("");
        // }
        #pragma omp critical
        {
            if (hash_table.find(key) == hash_table.end()) {
                // NOTE : 预分配64大小
                std::vector<ui> temp;
                temp.push_back(i);
                hash_table[std::move(key)] = std::move(temp);
            } else {
                hash_table[std::move(key)].push_back(i);
            }
        }
    }

    if (checkOverTime()) {
        return handleResp();
    }
    // auto end = std::chrono::steady_clock::now();
    // double single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // LOG() << "build_hashtable_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;
    // log the hash_table
    // oss << "log the hash_table: ";
    // for (auto& [k, v] : hash_table) {
    //     oss << k->toString() << " -> ";
    //     for (auto& idx : v) {
    //         oss << idx << ", ";
    //     }
    //     oss << std::endl;
    // }
    // LOG() << oss.str();
    // oss.str("");


    // LOG() << "hashtable bucket size == " << hash_table.bucket_count() << std::endl;
    // LOG() << "hashtable size == " << hash_table.size() << std::endl;
    // printHashTable(hash_table);

    // 探测阶段
    // auto findstart = std::chrono::steady_clock::now();
    // auto findend = std::chrono::steady_clock::now();

    // auto constrcutstart = std::chrono::steady_clock::now();
    // auto constrcutend = std::chrono::steady_clock::now();

    // start = std::chrono::steady_clock::now();
    
    // 探测阶段：并行探测
    auto start = std::chrono::steady_clock::now();

    bool exit_flag = false;
    ui total_size = 0;
    #pragma omp parallel  // 创建线程组
    {

        PartialMatch* local_joined = new PartialMatch(joined->length, R->total_vertices_count, R->size + S->size);
        // 每个线程的 visited 和 visited_flag 是独立的
        std::vector<bool> visited(data_graph_vcnt, false);  // 初始化为 false，根据需要调整大小
        std::vector<ui> visited_flag(data_graph_vcnt);   // 初始化为 0，根据需要调整大小

        std::copy(S->new2olds, S->new2olds + S->length, local_joined->new2olds);
        std::copy(S->old2news, S->old2news + S->total_vertices_count, local_joined->old2news);
        ui now_length = S->length;
        for (ui i = 0; i < R->length; ++i) {
            if (RjoinIdxSet.find(i) != RjoinIdxSet.end()) {
                continue;
            }
            // LOG() << "add R's idx " << i << ", old_id == " << R->new2olds[i] << std::endl;
            local_joined->new2olds[now_length] = R->new2olds[i];
            local_joined->old2news[R->new2olds[i]] = now_length;
        }

        #pragma omp for
        for (uint64_t i = 0; i < S->getSize(); ++i) {
            if (exit_flag) continue;

            // if (local_joined->size >= 1000) {
            //     #pragma omp critical
            //     {
            //         total_size += local_joined->size;
            //         LOG() << "total_size == " << total_size << std::endl;
            //         ui total_length = total_size * joined->length;
            //         // 调整目标容器的大小
            //         joined->data.resize(total_length);
        
            //         std::copy(local_joined->data.begin(),
            //             local_joined->data.begin() + local_joined->size * local_joined->length,
            //             joined->data.begin() + (total_size - local_joined->size) * local_joined->length
            //         );
                    
            //         joined->size += local_joined->size;  // 更新大小
            //         local_joined->size = 0;
            //     }

            // }

            // findstart = std::chrono::steady_clock::now();
            if (checkOverTime()) {
                // LOG() << "processing " << i << " of " << S->getSize() << std::endl;
                // LOG() << "overtime, exit" << std::endl;
                exit_flag = true;
            }
            // S->logSingleRow(i);
            uint32_t* data = S->getTuple(i);
            std::unique_ptr<JoinKey> key = create_join_key(SjoinIdxs.size());
            key->construct(data, SjoinIdxs);
            auto iter = hash_table.find(key);

            // if (i == 95) {
            //     oss << "log 95-th S data: ";
            //     for (ui j = 0; j < S->length; ++j) {
            //         oss << data[j] << ", ";
            //     }
            //     oss << std::endl;
            //     oss << "95-th key == " << key->toString() << std::endl;
            //     LOG() << oss.str();
            //     oss.str("");
            // }

            // findend = std::chrono::steady_clock::now();
            // find_key_time += std::chrono::duration_cast<std::chrono::nanoseconds>(findend - findstart).count();

            // constrcutstart = std::chrono::steady_clock::now();
            if (iter != hash_table.end()) {
                if (checkOverTime()) {
                    exit_flag = true;
                }
                // LOG() << key->toString() << " found, idxs.size == " << iter->second.size() << std::endl;
                std::vector<uint32_t>& row_indices = iter->second;
                for (auto& row_index : row_indices) {
                    // LOG() << "R row == " << row_index << ", S row == " << i << std::endl;
                    // auto exactstart = std::chrono::steady_clock::now();
                    constructRow(row_index, i, visited, visited_flag, local_joined);
                    // auto exactend = std::chrono::steady_clock::now();
                    // exact_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(exactend - exactstart).count();
                }
            } else {
                // LOG() << key->toString() << ", not found." << std::endl;
            }
            // constrcutend = std::chrono::steady_clock::now();
            // construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(constrcutend - constrcutstart).count();
        }

        // 在每个线程的循环结束前，将其local_joined数据合并到global joined中
        // 这里假设 joined 是线程安全的或你已经在其他地方进行了同步
        // 使用 std::move 将 local_joined 移动到 joined 中
        #pragma omp critical
        {
            total_size += local_joined->size;
            // LOG() << "total_size == " << total_size << std::endl;
            ui total_length = total_size * joined->length;
            // 调整目标容器的大小
            joined->data.resize(total_length);

            std::move(local_joined->data.begin(),
                local_joined->data.begin() + local_joined->size * local_joined->length,
                joined->data.begin() + (total_size - local_joined->size) * local_joined->length
            );
            
            joined->size += local_joined->size;  // 更新大小

            // // 这里将 local_joined 的数据合并到 joined 中
            // joined->data.insert(joined->data.end(), std::make_move_iterator(local_joined->data.begin()), std::make_move_iterator(local_joined->data.end()));
            // delete local_joined;  // 不要忘记释放内存
        }

        // 清理内存
        delete local_joined;  // 不要忘记释放内存
        
    }

    // auto thread_end = std::chrono::steady_clock::now();
    // double thread_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(thread_end - thread_start).count();
    // oss.str("");
    // oss << "joinround: " << joinround << ", ";
    // oss << "thread_id == " << 0 << ", ";
    // oss << "thread_hashjoin_time_in_ns == " << intTimer::TEMPNANOSECTOSEC(thread_hashjoin_time_in_ns);
    // oss << ", thread_round_result.size == " << joined->size << std::endl;
    // LOG() << oss.str();
    // LOG() << "result.length == " << joined->length << std::endl;
    // LOG() << "result.size == " << joined->data.size() << std::endl;

    // auto end = std::chrono::steady_clock::now();
    // auto single_hashjoin_time_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    // LOG() << "probe_time == " << intTimer::TEMPNANOSECTOSEC(single_hashjoin_time_in_ns) << " s" << std::endl;

    return joined;
}

void
NoPartitionHashJoin::restoreVisited(std::vector<bool>& visited, std::vector<ui>& visited_flag, size_t& visited_cnt) {
    for (ui jj = 0; jj < visited_cnt; jj++) {
        visited[visited_flag[jj]] = false;
    }
    visited_cnt = 0;
}

void
NoPartitionHashJoin::restoreVisitedSingle(std::vector<bool>& local_visited, std::vector<ui>& local_visited_flag, size_t& local_visited_cnt) {
    for (ui jj = 0; jj < local_visited_cnt; jj++) {
        local_visited[local_visited_flag[jj]] = false;
    }
    local_visited_cnt = 0;
}

// 应该对length较大的用memcpy，也就是S，而R用赋值
void
NoPartitionHashJoin::constructRow(ui R_row_idx, ui S_row_idx, std::vector<bool>& visited, std::vector<ui>& visited_flag, PartialMatch* joined) {
    size_t visited_cnt = 0;
    // auto totalstart = std::chrono::steady_clock::now();
    // auto totalend = std::chrono::steady_clock::now();
    // auto start = std::chrono::steady_clock::now();
    std::ostringstream oss;

    // oss << "call constuctRow" << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    // 边界检查
    assert(R_row_idx < R->size);
    assert(S_row_idx < S->size);

    // 计算需要的最小容量
    // size_t required_size = (joined->size + 1) * joined->length;
    // // LOG() << "required_size == " << required_size << std::endl;

    // // 自动扩容逻辑：如果容量不足，扩容为当前需求的两倍
    // if (joined->data.size() < required_size) {
    //     size_t new_size = std::max(joined->data.size() * 8, required_size);
    //     // LOG() << "new_size == " << new_size << std::endl;
    //     joined->data.resize(new_size);
    // }
    // auto end = std::chrono::steady_clock::now();
    // extend_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // LOG() << "start copy R" << std::endl;

    // auto start1 = std::chrono::steady_clock::now();
    // 复制S的match部分
    auto st = S->data.begin() + S_row_idx * S->length;
    auto ed = S->data.begin() + (S_row_idx + 1) * S->length;
    std::vector<ui> local_joined(joined->length);
    // auto target = joined->data.begin() + joined->length * joined->size;
    auto target = local_joined.begin();

    for (auto ite = st; ite != ed; ite++) {
        // LOG() << "*ite == " << *ite << std::endl;
        visited[*ite] = true;
        visited_flag[visited_cnt++] = *ite;
    }
    // NOTE: copy是不会自动扩容的
    std::copy(st, ed, target);
    target += S->length; 
    // auto end1 = std::chrono::steady_clock::now();
    // copy_S_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();

    // LOG () << "in concatenate" << std::endl;

    // LOG() << "start copy S" << std::endl;
    // 这里只能手动复制了
    /*
        RjoinIdxs = {0, 1, 4, 6}
        round 1:
            t = 0, idx = 0
            do nothing
        round 2:
            t = 1, idx = 1
            do nothing
        round 3:
            t = 2, idx = 2
            idx < RjoinIdxs[2]==4, copy
            t = 2, idx = 3
            idx < RjoinIdxs[2]==4, copy
            t = 2, idx = 4
            do nothing
        roud 4:
            t = 3, idx = 5
            idx < RjoinIdxs[3]==6, copy
            t = 3, idx = 6
            do nothing
     */

    // oss << "RjoinIdxs : " << std::endl;
    // for (auto& i : RjoinIdxs) {
    //     oss << i << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // LOG() << "R->length == " << R->length << std::endl;
    // while(idx < R->length) {
    //     LOG() << "idx == " << idx << std::endl;
    //     // 打印每一轮开始时的 t 和 idx
    //     // oss.str("");  // 清空之前的内容
    //     // oss << "Round " << t + 1 << ": t = " << t << ", idx = " << idx << std::endl;
    //     // LOG() << oss.str();
    //     for (; idx < RjoinIdxs[t]; ++idx) {
    //         // oss.str("");  // 清空之前的内容
    //         // oss << "idx " << idx << " < RjoinIdxs[" << t << "]==" << RjoinIdxs[t] << ", copy" << std::endl;
    //         // LOG() << oss.str();
    //         if (visited[R->data[Rst + idx]]) {
//             LOG() << R->data[Rst + idx] << " has been visited 1" << std::endl;
//             restoreVisited();
//             // for (auto ite = st; ite != ed; ite++) {
//             //     LOG() << "visited[" << *ite << "] == " << visited[*ite] << std::endl;
//             // }
//             return;
//         }
//         *target = R->data[Rst + idx];
//         LOG() << "add new value 1 " << *target << std::endl;
//         target++;
            
    //     }

    //     // NOTE: 因为要检查 conflict，这个优化被ban了
    //     if (++t >= RjoinIdxs[RjoinIdxs.size() - 1]) {
    //         for (; idx < R->length; ++idx) {
    //             if (visited[R->data[Rst + idx]]) {
    //                 LOG() << R->data[Rst + idx] << " has been visited 2" << std::endl;
    //                 restoreVisited();
    //                 return;
    //             }
    //             *target = R->data[Rst + idx];
    //             LOG() << "add new value 2 " << *target << std::endl;
    //             target++;
    //         }
    //         // std::copy(R->data.begin() + idx, R->data.begin() + Rst + R->length, target);
    //         break;
    //     }
    // }

    // auto start2 = std::chrono::steady_clock::now();
    // auto end2 = std::chrono::steady_clock::now();
    ui idx = 0;
    ui Rst = R_row_idx * R->length;

    while(idx < R->length) {
        if (RjoinIdxSet.find(idx) != RjoinIdxSet.end()) {
            idx++;
            continue;
        }
        if (visited[R->data[Rst + idx]]) {
            // LOG() << R->data[Rst + idx] << " has been visited" << std::endl;
            restoreVisited(visited, visited_flag, visited_cnt);
            // end2 = std::chrono::steady_clock::now();
            // copy_R_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
            // 要把S和R的时间都算上，才是visited浪费的时间
            // copy_visited_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start1).count();
            // totalend = std::chrono::steady_clock::now();
            // total_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(totalend - totalstart).count();
            // LOG() << "visited fail." << std::endl;
            return;
        }
        *target = R->data[Rst + idx];
        // LOG() << "add new value " << *target << std::endl;
        target++;
        idx++;
    }
    restoreVisited(visited, visited_flag, visited_cnt);



    // 合并到joined数据结构
    // 假设每个线程都已经准备好自己的结果
    // 计算需要的最小容量
    // std::lock_guard<std::mutex> lock(joined_mutex); // 在合并时使用锁保护共享数据
    size_t required_size = (joined->size + 1) * joined->length;
    if (joined->data.size() < required_size) {
        size_t new_size = std::max(joined->data.size() * 8, required_size);
        joined->data.resize(new_size);
    }
    ui insertIdx = joined->length * joined->size;
    joined->size += 1;
    std::copy(local_joined.begin(), local_joined.end(), joined->data.begin() + insertIdx);

    // oss << "get embedding : ";
    // for (ui i = 0; i < joined->length; ++i) {
    //     oss << joined->data[(joined->size - 1) * joined->length + i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    // end2 = std::chrono::steady_clock::now();
    // copy_R_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    // oss.str("");
    // oss << "new line : ";
    // for (ui i = 0; i < joined->length; ++i) {
    //     oss << joined->data[(joined->size - 1) * joined->length + i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // totalend = std::chrono::steady_clock::now();
    // total_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(totalend - totalstart).count();
    return;
}

// 要在这里面把R和S都换成local的结构
// 应该对length较大的用memcpy，也就是S，而R用赋值
void
NoPartitionHashJoin::constructRowSingle(ui R_row_idx, ui S_row_idx, size_t thread_id,
        std::vector<bool>& local_visited, std::vector<ui>& local_visited_flag,
        PartialMatch* local_R, PartialMatch* local_S) {
    // auto totalstart = std::chrono::steady_clock::now();
    // auto totalend = std::chrono::steady_clock::now();
    auto start = std::chrono::steady_clock::now();
    std::ostringstream oss;

    // 边界检查
    assert(R_row_idx < local_R->size);
    assert(S_row_idx < local_S->size);

    PartialMatch* res_joined = local_joined_s[thread_id];

    // 计算需要的最小容量
    size_t required_size = (res_joined->size + 1) * res_joined->length;
    // LOG() << "required_size == " << required_size << std::endl;

    // 自动扩容逻辑：如果容量不足，扩容为当前需求的8倍
    if (res_joined->data.size() < required_size) {
        size_t new_size = std::max(res_joined->data.size() * 8, required_size);
        // LOG() << "new_size == " << new_size << std::endl;
        res_joined->data.resize(new_size);
    }
    auto end = std::chrono::steady_clock::now();
    extend_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // LOG() << "start copy R" << std::endl;

    auto start1 = std::chrono::steady_clock::now();
    // 复制S的match部分
    auto st = local_S->data.begin() + S_row_idx * local_S->length;
    auto ed = local_S->data.begin() + (S_row_idx + 1) * local_S->length;
    auto target = res_joined->data.begin() + res_joined->length * res_joined->size;

    size_t local_visited_cnt = 0;
    for (auto ite = st; ite != ed; ite++) {
        // LOG() << "*ite == " << *ite << std::endl;
        local_visited[*ite] = true;
        local_visited_flag[local_visited_cnt++] = *ite;
    }
    // NOTE: copy是不会自动扩容的
    std::copy(st, ed, target);
    target += local_S->length;
    auto end1 = std::chrono::steady_clock::now();
    copy_S_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();

    // LOG () << "in concatenate" << std::endl;

    // LOG() << "start copy local_S" << std::endl;
    // 这里只能手动复制了
    /*
        RjoinIdxs = {0, 1, 4, 6}
        round 1:
            t = 0, idx = 0
            do nothing
        round 2:
            t = 1, idx = 1
            do nothing
        round 3:
            t = 2, idx = 2
            idx < RjoinIdxs[2]==4, copy
            t = 2, idx = 3
            idx < RjoinIdxs[2]==4, copy
            t = 2, idx = 4
            do nothing
        roud 4:
            t = 3, idx = 5
            idx < RjoinIdxs[3]==6, copy
            t = 3, idx = 6
            do nothing
     */

    // oss << "RjoinIdxs : " << std::endl;
    // for (auto& i : RjoinIdxs) {
    //     oss << i << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");

    // LOG() << "local_R->length == " << local_R->length << std::endl;
    // while(idx < local_R->length) {
    //     LOG() << "idx == " << idx << std::endl;
    //     // 打印每一轮开始时的 t 和 idx
    //     // oss.str("");  // 清空之前的内容
    //     // oss << "Round " << t + 1 << ": t = " << t << ", idx = " << idx << std::endl;
    //     // LOG() << oss.str();
    //     for (; idx < RjoinIdxs[t]; ++idx) {
    //         // oss.str("");  // 清空之前的内容
    //         // oss << "idx " << idx << " < RjoinIdxs[" << t << "]==" << RjoinIdxs[t] << ", copy" << std::endl;
    //         // LOG() << oss.str();
    //         if (visited[local_R->data[Rst + idx]]) {
    //             LOG() << local_R->data[Rst + idx] << " has been visited 1" << std::endl;
    //             restoreVisited();
    //             // for (auto ite = st; ite != ed; ite++) {
    //             //     LOG() << "visited[" << *ite << "] == " << visited[*ite] << std::endl;
    //             // }
    //             return;
    //         }
    //         *target = local_R->data[Rst + idx];
    //         LOG() << "add new value 1 " << *target << std::endl;
    //         target++;
            
    //     }

    //     // NOTE: 因为要检查 conflict，这个优化被ban了
    //     if (++t >= RjoinIdxs[RjoinIdxs.size() - 1]) {
    //         for (; idx < local_R->length; ++idx) {
    //             if (visited[local_R->data[Rst + idx]]) {
    //                 LOG() << local_R->data[Rst + idx] << " has been visited 2" << std::endl;
    //                 restoreVisited();
    //                 return;
    //             }
    //             *target = local_R->data[Rst + idx];
    //             LOG() << "add new value 2 " << *target << std::endl;
    //             target++;
    //         }
    //         // std::copy(local_R->data.begin() + idx, local_R->data.begin() + Rst + local_R->length, target);
    //         break;
    //     }
    // }

    // auto start2 = std::chrono::steady_clock::now();
    // auto end2 = std::chrono::steady_clock::now();
    ui idx = 0;
    ui Rst = R_row_idx * local_R->length;

    while(idx < local_R->length) {
        if (RjoinIdxSet.find(idx) != RjoinIdxSet.end()) {
            idx++;
            continue;
        }

        if (local_visited[local_R->data[Rst + idx]]) {
            // LOG() << local_R->data[Rst + idx] << " has been visited" << std::endl;
            restoreVisitedSingle(local_visited, local_visited_flag, local_visited_cnt);
            // end2 = std::chrono::steady_clock::now();
            // copy_R_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
            // // 要把S和R的时间都算上，才是visited浪费的时间
            // copy_visited_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start1).count();
            // totalend = std::chrono::steady_clock::now();
            // total_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(totalend - totalstart).count();
            // LOG() << "visited fail." << std::endl;
            return;
        }
        *target = local_R->data[Rst + idx];
        // LOG() << "add new value " << *target << std::endl;
        target++;
        idx++;
    }
    restoreVisitedSingle(local_visited, local_visited_flag, local_visited_cnt);
    res_joined->size += 1;
    // oss << "get embedding : ";
    // for (ui i = 0; i < res_joined->length; ++i) {
    //     oss << res_joined->data[(res_joined->size - 1) * res_joined->length + i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // oss.str("");
    // end2 = std::chrono::steady_clock::now();
    // copy_R_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    // oss.str("");
    // oss << "new line : ";
    // for (ui i = 0; i < res_joined->length; ++i) {
    //     oss << res_joined->data[(res_joined->size - 1) * res_joined->length + i] << ", ";
    // }
    // oss << std::endl;
    // LOG() << oss.str();
    // totalend = std::chrono::steady_clock::now();
    // total_construct_time += std::chrono::duration_cast<std::chrono::nanoseconds>(totalend - totalstart).count();
    return;
}

PartialMatch*
NoPartitionHashJoin::handleResp() {
    return joined;
}

bool
NoPartitionHashJoin::checkOverTime() {
    if (intTimer::getClockNano() >= time_limit) {
        return true;
    }
    return false;
}
