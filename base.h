#pragma once

#include <string>
#include <string_view>
#include <cstdint>      // 用于 uint32_t, uint64_t
#include <cstring>      // 用于 memcpy
#include <stdexcept>    // (可选) 用于错误处理

// --- V2 布局常量 ---

// 我们的演示用数据块大小阈值 (真实世界是 4KB+)
const uint32_t DATA_BLOCK_SIZE_THRESHOLD = 128; // 128 字节

// 用于校验 SSTable 文件的“魔数”
const uint64_t SSTABLE_MAGIC_NUMBER = 0xDEADBEEFCAFEF00D;

/**
 * @brief BlockHandle (块句柄) - "数据块的指针"
 * 磁盘布局: [offset (8 字节)] [size (4 字节)]
 */
struct BlockHandle {
    uint64_t offset_ = 0;
    uint32_t size_ = 0;

    /**
     * @brief 【EncodeTo 实现】
     * 将此结构体序列化（扁平化）为一个12字节的序列，并追加到 dst
     */
    void EncodeTo(std::string* dst) const {
        dst->append(reinterpret_cast<const char*>(&offset_), sizeof(offset_));
        dst->append(reinterpret_cast<const char*>(&size_), sizeof(size_));
    }

    /**
     * @brief 【DecodeFrom 实现】
     * 从 input (一个字节视图) 的开头解析12字节，填充此结构体
     */
    bool DecodeFrom(std::string_view* input) {
        if (input->size() < (sizeof(offset_) + sizeof(size_))) {
            return false; // 字节不够
        }
        memcpy(&offset_, input->data(), sizeof(offset_));
        input->remove_prefix(sizeof(offset_));
        memcpy(&size_, input->data(), sizeof(size_));
        input->remove_prefix(sizeof(size_));
        return true;
    }
};

const uint32_t BLOCK_HANDLE_SIZE = sizeof(uint64_t) + sizeof(uint32_t); // 12 字节

/**
 * @brief Footer (文件尾) - "索引块的指针"
 * 磁盘布局: [index_block_handle (12B)] [magic_number (8B)]
 */
struct Footer {
    BlockHandle index_block_handle_; // 指向 Index Block
    uint64_t magic_number_ = 0;      // 魔数

    /**
     * @brief 【EncodeTo 实现】
     * 将此结构体序列化（扁平化）为一个20字节的序列，并追加到 dst
     */
    void EncodeTo(std::string* dst) const {
        index_block_handle_.EncodeTo(dst);
        dst->append(reinterpret_cast<const char*>(&magic_number_), sizeof(magic_number_));
    }

    /**
     * @brief 【DecodeFrom 实现】
     * 从 input (一个20字节的视图) 中解析，填充此结构体
     */
    bool DecodeFrom(std::string_view input) {
        if (input.size() < (BLOCK_HANDLE_SIZE + sizeof(magic_number_))) {
            return false;
        }
        // 先校验魔数 (魔数在 handle 之后)
        memcpy(&magic_number_, input.data() + BLOCK_HANDLE_SIZE, sizeof(magic_number_));
        if (magic_number_ != SSTABLE_MAGIC_NUMBER) {
            return false; // 这不是一个有效的 SSTable 文件
        }
        // 魔数正确，现在解析 handle
        std::string_view handle_input = input.substr(0, BLOCK_HANDLE_SIZE);
        return index_block_handle_.DecodeFrom(&handle_input);
    }
};

const uint32_t FOOTER_SIZE = BLOCK_HANDLE_SIZE + sizeof(uint64_t); // 20 字节

// --- 内部 K/V 格式辅助函数 ---
// Data Block 和 Index Block 内部都使用这个简单的 K/V 格式
// [key_len (4B)] [key_data] [val_len (4B)] [val_data]

/**
 * @brief 将一个 K/V 对追加到缓冲区
 */
inline void writeKV(std::string* buffer, std::string_view key, std::string_view value) {
    uint32_t key_len = static_cast<uint32_t>(key.size());
    uint32_t value_len = static_cast<uint32_t>(value.size());
    buffer->append(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    buffer->append(key.data(), key.size());
    buffer->append(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    buffer->append(value.data(), value.size());
}

/**
 * @brief 帮助计算一个 K/V 记录在磁盘上的确切大小
 */
inline uint32_t getEntrySize(std::string_view key, std::string_view value) {
    // (key_len) + (key_data) + (val_len) + (val_data)
    return sizeof(uint32_t) + key.size() + sizeof(uint32_t) + value.size();
}

/**
 * @brief 尝试从 input 缓冲区中读取一个 K/V 对（并从 input 中移除）
 */
inline bool readKV(std::string_view* input, std::string_view* key, std::string_view* value) {
    uint32_t key_len = 0;
    if (input->size() < sizeof(key_len)) return false;
    memcpy(&key_len, input->data(), sizeof(key_len));
    input->remove_prefix(sizeof(key_len));

    if (input->size() < key_len) return false;
    *key = std::string_view(input->data(), key_len);
    input->remove_prefix(key_len);

    uint32_t value_len = 0;
    if (input->size() < sizeof(value_len)) return false;
    memcpy(&value_len, input->data(), sizeof(value_len));
    input->remove_prefix(sizeof(value_len));

    if (input->size() < value_len) return false;
    
    // 【修复】
    // *value = std::string_view(input->data(), value.size()); // <-- 错误！
    *value = std::string_view(input->data(), value_len); // <-- 正确！

    input->remove_prefix(value_len);

    return true;
}