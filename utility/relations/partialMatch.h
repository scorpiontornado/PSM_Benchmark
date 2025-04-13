#ifndef _PARTIALMATCH_H
#define _PARTIALMATCH_H

#include <vector>
#include "configuration/types.h"

// - matches: 一维数组存储table
// - size: 当前匹配的数量
// - length: 每个匹配（表格每一行）的大小
// - new2old和old2new的映射（根据具体实现）
class PartialMatch {
  public:
    PartialMatch() = default;

    PartialMatch(ui length, ui total_vcnt, ui init_capacity=0);

    // 添加一个 partial match，输入为一个包含 N 个元素的 vector
    void add(const ui* match);

    ui getSize();
    ui* getTuple(ui row_idx);

    void logTop10();

    void logAll();
    void logAll2();

    void logSingleRow(ui row_idx);

    ~PartialMatch();

    std::vector<ui> data;
    ui size{0};
    // ui** matches{nullptr};
    ui length{0};
    ui init_capacity{0};
    ui total_vertices_count{0};

    ui* old2news{nullptr};
    ui* new2olds{nullptr};
};

#endif // _PARTIALMATCH_H