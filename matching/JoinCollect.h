#include "graph/graph.h"
#include "TaskPool.h"
#include "utility/splitGraph/SplitGraph.h"
#include "utility/relations/partialMatch.h"
#include "utility/statistics/intTimer.h"
#include "utility/hashjoin/hashjoin.h"

// Hash Join 实现
class JoinCollect {
  public:
    JoinCollect(Graph* data_graph, Graph* g, JoinTree* jtree, ui cnt, int64_t time_limit, TaskPool* taskPool);

    JoinCollect(Graph* data_graph, Graph* g, JoinSequence* jseq, ui cnt, int64_t time_limit, TaskPool* taskPool);

    ~JoinCollect();

    /*
        collect all the keys of R table
        begin_idx : the offset of the i-th row in the partial match table,
        matches : R table,
        joinKeys : the index of joinKeys, like [0, 1, 3, 5], not the uid
        key : Result
    */
    void getRowKey(ui begin_idx, std::vector<ui>& matches, std::set<ui>& joinKeys, std::vector<ui>& key);
    PartialMatch* concateTwoTabels(PartialMatch* R, PartialMatch* S);
    PartialMatch* recur_collect(ui u);
    PartialMatch* sequence_collect();
    bool checkOverTime();
    PartialMatch* handleResp();

  public:
    // subgraph_id : 0~K
    PartialMatch** partial_matches{nullptr};
    
    Graph* data_graph{nullptr};
    Graph* graph{nullptr};
    JoinTree* jtree{nullptr};
    JoinSequence* jseq{nullptr};
    ui unit_cnt{0};
    int64_t time_limit;
    bool overtime{false};
    bool* visited{nullptr};
    ui* visited_flag{nullptr};
    ui visited_cnt{0};

    TaskPool* taskPool{nullptr};
    int joinround{1};
    ui unit_count{0};
};
