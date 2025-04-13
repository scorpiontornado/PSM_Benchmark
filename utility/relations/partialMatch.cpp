#include "partialMatch.h"
#include "utility/statistics/Logs.h"

PartialMatch::PartialMatch(ui length, ui total_vcnt, ui init_capacity)
    : length(length), init_capacity(init_capacity), total_vertices_count(total_vcnt) {
    data.resize(length * init_capacity);
    old2news = new ui[total_vcnt];
    new2olds = new ui[total_vcnt];
}

PartialMatch::~PartialMatch() {
    delete[] old2news;
    delete[] new2olds;    
}

void
PartialMatch::add(const ui* match) {
    // assert(match.size() == length && "Match size does not equal length");
    data.insert(data.end(), match, match + length);
    size++;
}

ui
PartialMatch::getSize() {
    return size;
}

ui*
PartialMatch::getTuple(ui row_idx) {
    return data.data() + row_idx * length;
}

void
PartialMatch::logTop10() {
    std::ostringstream oss;
    oss << "Top 10 rows: " << std::endl;
    for (ui i = 0; i < std::min(size, (ui)10); ++i) {
        oss << "Row " << i << ": ";
        for (ui j = 0; j < length; ++j) {
            oss << data[i * length + j] << ", ";
        }
        oss << std::endl;
    }
    LOG() << oss.str();
}

void
PartialMatch::logAll() {
    std::ostringstream oss;
    oss << "original uids: ";
    for (ui i = 0; i < length; ++i) {
        oss << new2olds[i] << ", ";
    }
    oss << std::endl;
    oss << "All rows: " << std::endl;
    for (ui i = 0; i < size; ++i) {
        oss << "Row " << i << ": ";
        for (ui j = 0; j < length; ++j) {
            oss << data[i * length + j] << ", ";
        }
    }
    LOG() << oss.str();
}

void PartialMatch::logAll2() {
    // 存储每一行的字符串
    std::vector<std::string> lines;
    lines.reserve(size);

    // 1. 将每行数据记录到临时的 string 流中，然后存入 lines
    for (ui i = 0; i < size; ++i) {
        std::ostringstream line_oss;
        for (ui j = 0; j < total_vertices_count; ++j) {
            ui old_idx = old2news[j];
            line_oss << data[i * length + old_idx] << ", ";
        }
        // 把该行转成字符串放入容器
        lines.push_back(line_oss.str());
    }

    // 2. 对所有行做字典序排序
    std::sort(lines.begin(), lines.end()); 
    // 如果想要数值排序而不是纯字符串字典序，需要根据需求手动写比较函数

    // 3. 统一输出
    std::ostringstream oss;
    oss << "All rows (sorted by dictionary order):" << std::endl;
    for (auto &line : lines) {
        oss << line << std::endl;
    }

    LOG() << oss.str();
}


void
PartialMatch::logSingleRow(ui row_idx) {
    std::ostringstream oss;
    oss << "Single Row " << row_idx << ": ";
    for (ui j = 0; j < length; ++j) {
        oss << data[row_idx * length + j] << ", ";
    }
    oss << std::endl;
    LOG() << oss.str();
}

