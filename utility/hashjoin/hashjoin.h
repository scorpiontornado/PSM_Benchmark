#ifndef _NOPARTITIONHASHJOIN_H
#define _NOPARTITIONHASHJOIN_H

#include <vector>
#include <unordered_set>
#include <memory>
#include <omp.h>
#include "utility/relations/partialMatch.h"
#include "keyTraits.h"

class NoPartitionHashJoin {
  public:
    NoPartitionHashJoin(ui data_graph_vcnt, PartialMatch* Rori, PartialMatch* Sori,
      std::unordered_set<ui>& RjoinIdxSet, std::unordered_set<ui>& SjoinIdxSet, ui new_length,
      int64_t time_limit,  ui d_graph_vcnt,
      bool* visited, ui* visited_flag, ui thread_num, int joinround);


    PartialMatch* execute();
    PartialMatch* execute_parallel();

    void restoreVisited(std::vector<bool>& visited, std::vector<ui>& visited_flag, size_t& visited_cnt);
    // void restoreVisitedSingle(size_t thread_id);
    void restoreVisitedSingle(std::vector<bool>& local_visited, std::vector<ui>& local_visited_flag, size_t& local_visited_cnt);
    void constructRow(ui R_row_idx, ui S_row_idx);
    void constructRow(ui R_row_idx, ui S_row_idx, std::vector<bool>& visited, std::vector<ui>& visited_flag, PartialMatch* joined);
    void constructRowSingle(ui R_row_idx, ui S_row_idx, size_t thread_id,
      std::vector<bool>& local_visited, std::vector<ui>& local_visited_flag,
      PartialMatch* local_R, PartialMatch* local_S);

    std::unique_ptr<JoinKey> create_join_key(size_t key_size);

    PartialMatch* handleResp();

    bool checkOverTime();

  public:
    ui data_graph_vcnt{0};

    PartialMatch* R;
    PartialMatch* S;
    PartialMatch* joined;
    std::vector<PartialMatch*> local_joined_s;

    std::vector<ui> RjoinIdxs;
    std::vector<ui> SjoinIdxs;
    std::unordered_set<ui> RjoinIdxSet;
    std::unordered_set<ui> SjoinIdxSet;

    int64_t time_limit;

    std::mutex joined_mutex;

    ui thread_num{1};

    int joinround{1};

    double find_key_time{0.0};
    double construct_time{0.0};
    double total_construct_time{0.0};
    double exact_construct_time{0.0};
    double extend_time{0.0};
    double copy_S_time{0.0};
    double copy_R_time{0.0};
    double copy_visited_time{0.0};
};

#endif // _NOPARTITIONHASHJOIN_H