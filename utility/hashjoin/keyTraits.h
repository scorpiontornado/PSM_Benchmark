#ifndef _KEYTRAITS_H
#define _KEYTRAITS_H

#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

inline uint64_t murmur3(uint64_t val) {
    val ^= val >> 33;
    val *= 0xff51afd7ed558ccd;
    val ^= val >> 33;
    val *= 0xc4ceb9fe1a85ec53;
    val ^= val >> 33;
    return val;
}

inline uint32_t murmur3(uint32_t val) {
    val ^= val >> 16;
    val *= 0x85ebca6b;
    val ^= val >> 13;
    val *= 0xc2b2ae35;
    val ^= val >> 16;
    return val;
}

class JoinKey {
public:
    virtual ~JoinKey() = default;

    virtual size_t hash() const = 0;
    virtual bool equals(const JoinKey& other) const = 0;
    virtual void construct(const uint32_t* data, const std::vector<uint32_t>& indices) = 0;
    virtual std::string toString() const = 0; // 添加虚函数 toString()
};

// FixedSizeJoinKey
template <typename KeyType>
class FixedSizeJoinKey : public JoinKey {
private:
    KeyType key_;

public:
    FixedSizeJoinKey() : key_(0) {}

    size_t hash() const override {
        if constexpr (std::is_same_v<KeyType, uint32_t>) {
            return murmur3(key_);
        } else if constexpr (std::is_same_v<KeyType, uint64_t>) {
            return murmur3(key_);
        } else if constexpr (std::is_same_v<KeyType, __uint128_t>) {
            uint64_t high = static_cast<uint64_t>(key_ >> 64);
            uint64_t low = static_cast<uint64_t>(key_);
            return murmur3(high) ^ (murmur3(low) + 0x9e3779b9 + (high << 6) + (high >> 2));
        }
    }

    bool equals(const JoinKey& other) const override {
        auto* other_key = dynamic_cast<const FixedSizeJoinKey<KeyType>*>(&other);
        return other_key && key_ == other_key->key_;
    }

    void construct(const uint32_t* data, const std::vector<uint32_t>& indices) override {
        key_ = 0;
        for (auto index : indices) {
            key_ = (key_ << 32) | data[index];
        }
    }

    std::string toString() const override {
        std::ostringstream oss;
        if constexpr (std::is_same_v<KeyType, __uint128_t>) {
            uint64_t high = static_cast<uint64_t>(key_ >> 64);
            uint64_t low = static_cast<uint64_t>(key_);
            oss << "FixedSizeJoinKey(0x" 
                << std::hex << high << low << std::dec << ")";
        } else {
            oss << "FixedSizeJoinKey(" << key_ << ")";
        }
        return oss.str();
    }

};

// ArrayJoinKey
class ArrayJoinKey : public JoinKey {
private:
    std::array<uint32_t, 8> key_;
    size_t key_size_;

public:
    ArrayJoinKey() : key_size_(0) {}

    size_t hash() const override {
        size_t hash = 0;
        for (size_t i = 0; i < key_size_; ++i) {
            hash ^= murmur3(key_[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    bool equals(const JoinKey& other) const override {
        auto* other_key = dynamic_cast<const ArrayJoinKey*>(&other);
        return other_key && key_size_ == other_key->key_size_ &&
               std::equal(key_.begin(), key_.begin() + key_size_, other_key->key_.begin());
    }

    void construct(const uint32_t* data, const std::vector<uint32_t>& indices) override {
        key_size_ = indices.size();
        for (size_t i = 0; i < key_size_; ++i) {
            key_[i] = data[indices[i]];
        }
    }

    std::string toString() const override {
        std::ostringstream oss;
        oss << "ArrayJoinKey(";
        for (size_t i = 0; i < key_size_; ++i) {
            if (i > 0) oss << ", ";
            oss << key_[i];
        }
        oss << ")";
        return oss.str();
    }
};

// VectorJoinKey
class VectorJoinKey : public JoinKey {
private:
    std::vector<uint32_t> key_;

public:
    size_t hash() const override {
        size_t hash = 0;
        for (auto val : key_) {
            hash ^= murmur3(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    bool equals(const JoinKey& other) const override {
        auto* other_key = dynamic_cast<const VectorJoinKey*>(&other);
        return other_key && key_ == other_key->key_;
    }

    void construct(const uint32_t* data, const std::vector<uint32_t>& indices) override {
        key_.resize(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            key_[i] = data[indices[i]];
        }
    }

    std::string toString() const override {
        std::ostringstream oss;
        oss << "VectorJoinKey(";
        for (size_t i = 0; i < key_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << key_[i];
        }
        oss << ")";
        return oss.str();
    }
};

#endif // KEYTRAITS_H
