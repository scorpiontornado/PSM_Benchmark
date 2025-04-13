# 查找 Google Test
find_package(GTest REQUIRED)

# 定义多个测试源文件
set(TEST_SOURCES
    testEvaluateQuery.cpp
    testSplitGraph.cpp
    testPartialMatch.cpp
    testFilter.cpp
    testHashJoin.cpp
)

# 创建一个测试的可执行文件（将多个源文件编译到一个测试程序中）
add_executable(runAllTests ${TEST_SOURCES})

# 链接库
target_link_libraries(
    runAllTests PUBLIC
    gtest_main
    subgraph_lib       # 链接 subgraph_lib 模块
    graph              # 链接 graph 模块
    utility            # 链接 utility 模块
)

# 添加测试
add_test(
    NAME runAllTests
    COMMAND runAllTests
)
