#include <unordered_map>
#include <vector>
#include <memory>  // for std::unique_ptr
#include "utility/statistics/Logs.h"

struct JoinKeyHash {
    size_t operator()(const std::unique_ptr<JoinKey>& key) const { return key->hash(); }
};

struct JoinKeyEqual {
    bool operator()(const std::unique_ptr<JoinKey>& lhs, const std::unique_ptr<JoinKey>& rhs) const {
        return lhs->equals(*rhs);
    }
};

using HashTable = std::unordered_map<
    std::unique_ptr<JoinKey>,  // Key 是 JoinKey 的智能指针
    std::vector<uint32_t>,     // Value 是存储行索引的 vector
    JoinKeyHash,               // 自定义哈希函数
    JoinKeyEqual               // 自定义比较函数
>;

// 打印 HashTable
void printHashTable(const HashTable& hash_table) {
    std::ostringstream oss;
    oss << "HashTable Contents:" << std::endl;

    for (const auto& [key, values] : hash_table) {
        // 打印键
        oss << "Key: " << key->toString() << " -> Values: [";

        // 打印值（vector）
        for (size_t i = 0; i < values.size(); ++i) {
            oss << values[i];
            if (i != values.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]" << std::endl;
    }
    LOG() << oss.str();
}
