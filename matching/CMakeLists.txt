set(SUBGRAPH_MATCHING_FILE_LIST
    matchingcommand.cpp
    matchingcommand.h
    FilterVertices.cpp
    FilterVertices.h
    BuildTable.cpp
    BuildTable.h
    GenerateQueryPlan.cpp
    GenerateQueryPlan.h
    EvaluateQuery.cpp
    EvaluateQuery.h
    GenerateFilteringPlan.cpp
    GenerateFilteringPlan.h
    TaskPool.cpp
    Materializer.cpp
    Args.cpp
    JoinCollect.cpp
)

set(GRAPH_CONVERTER_FILE_LIST
    GraphConverter.cpp)

add_library(subgraph_lib STATIC ${SUBGRAPH_MATCHING_FILE_LIST})
add_executable(SubgraphMatching.out StudyPerformance.cpp)
# 添加可执行文件 SubgraphMatching.out，并链接 OpenMP
target_link_libraries(SubgraphMatching.out 
    PRIVATE 
    subgraph_lib 
    graph 
    utility 
    OpenMP::OpenMP_CXX  # 仅在此处链接 OpenMP
)


add_executable(GraphConverter.out ${GRAPH_CONVERTER_FILE_LIST})
target_link_libraries(GraphConverter.out graph utility)

add_executable(EdgeListConverter.out EdgeListToCSR.cpp)
add_executable(ReassignLabel ReassignLabel.cpp)
add_executable(SelectEdge SelectEdge.cpp)
# add_subdirectory(test)