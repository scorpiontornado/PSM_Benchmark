#ifndef SUBGRAPHMATCHING_CONFIG_H
#define SUBGRAPHMATCHING_CONFIG_H

#define INVALID_VERTEX_ID 100000000

/**
 * Set the maximum size of a query graph. By default, we set the value as 64.
 */
#define MAXIMUM_QUERY_GRAPH_SIZE 64

/**
 * Setting the value as 1 is to (1) enable the neighbor label frequency filter (i.e., NLF filter); and (2) enable
 * to check the existence of an edge with the label information. The cost is to (1) build an unordered_map for each
 * vertex to store the frequency of the labels of its neighbor; and (2) build the label neighbor offset.
 * If the memory can hold the extra memory cost, then enable this feature to boost the performance. Otherwise, disable
 * it by setting this value as 0.
 */
#define OPTIMIZED_LABELED_GRAPH 1

/**
 * Define SPECTRUM to enable spectrum analysis.
 */
// #define SPECTRUM

/**
 * Set intersection method.
 * 0: Hybrid method; 1: Merge based set intersections.
 */
#define HYBRID 0

/**
 * Accelerate set intersection with SIMD instructions.
 * 0: AVX2; 1: AVX512; 2: Basic;
 */
#define SI 0

/**
 * Define ENABLE_QFLITER to enable QFliter set intersection method.
 */
// #define ENABLE_QFLITER 1

/**
 * Define ENABLE_FAILING_SET to enable the failing set pruning set intersection method.
 */
// #define ENABLE_FAILING_SET
inline int FAILING_SET_FLAG = 0;

/**
 * Define ENABLE_FROZEN_SET to enable the frozen set pruning set intersection method.
 */
#define ENABLE_FROZEN_SET
inline int FROZEN_SET_FLAG = 0;

/**
 * 对应 PSM 的 \beta 参数
*/
inline int MASSIVE_CANDIDATE_COUNT = 8;

inline int PREMATCH_MAX_SPLIT_DEPTH = 3;


inline int USING_CANDIDATE_INDEX_FLAG = 0;

/**
 * Enable collection the distribution of the results in the data graph.
 */

// #define DISTRIBUTION


#define PRINT_SEPARATOR "------------------------------"

#endif //SUBGRAPHMATCHING_CONFIG_H
