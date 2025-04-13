#ifndef CONCATENATE_H
#define CONCATENATE_H

#include "graph/graph.h"

// just for concatenating the partial results of different units
/*
    这里不包含任何拓扑/label信息，只保留partial matching以处理冲突
*/

class ConcatenateUnits {

  public:
    void joinTogether(); // may be recursive

  private:
    void left_deep_join();

    void two_way_bushy_join();

    void multi_way_join();
    
    void worstcase_optimal_join();
};





#endif // CONCATENATE_H