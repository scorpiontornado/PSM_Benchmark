set(UTILITY_SRC
        graphoperations.cpp
        commandparser.cpp
        commandparser.h
        computesetintersection.h
        computesetintersection.cpp
        bitsetoperation.cpp
        bitsetoperation.h
        QFliter.h
        han/intersection_algos.cpp
        han/utils/util.cpp
        statistics/Logs.cpp
        splitGraph/SplitGraph.cpp
        relations/partialMatch.cpp
        relations/partialMatch.h
        hashjoin/hashjoin.cpp
        hashjoin/hashjoin.h
)

add_library(utility SHARED
        ${UTILITY_SRC})

# 添加 OpenMP 链接
target_link_libraries(utility PRIVATE OpenMP::OpenMP_CXX)