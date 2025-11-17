#include "sstablebuilder.h"
#include <iostream>  // 用于打印调试信息
#include <cassert>   // 用于断言 (可选)

// 构造函数：初始化所有成员变量
SSTableBuilder::SSTableBuilder(const std::string& filename)
    : ofs_(filename, std::ios::binary | std::ios::trunc), // 清空并以二进制打开
      finished_(false),
      cur_data_block_offset_(0) { // 第一个块从 offset 0 开始
    if (!ofs_) {
        std::cerr << "错误: SSTableBuilder 无法打开文件 " << filename << std::endl;
    }
}

// 析构函数：确保文件关闭 (即使 Finish() 没有被调用)
SSTableBuilder::~SSTableBuilder() {
    if (ofs_.is_open() && !finished_) {
        // 如果用户忘了调用 Finish()，我们至少要关闭文件
        // 尽管这会导致一个损坏的/不完整的 SSTable
        ofs_.close();
    }
}

/**
 * @brief 添加 K/V，并在必要时触发 Data Block 刷盘
 */
bool SSTableBuilder::Add(std::string_view key, std::string_view value) {
    if (finished_ || !ofs_) return false; // 检查状态

    // 检查 Key 必须是升序的 (防止逻辑错误)
    if (!last_key_in_block_.empty() && cur_data_block_.empty() && key <= last_key_in_block_) {
        // 只有在开启一个 *新* 块时，才需要和 *上一个* 块的 last_key 比较
        std::cerr << "错误: Key 必须按全局升序添加。" << std::endl;
        return false;
    }

    // 1. 预计算大小 (函数来自 base.h)
    uint32_t entry_size = getEntrySize(key, value); 

    // 2. 检查是否需要切分
    if (cur_data_block_.empty()) {
        // 这是块的第一个条目，记录“便签”：它的起始位置
        cur_data_block_offset_ = static_cast<uint64_t>(ofs_.tellp());
    } else if (cur_data_block_.size() + entry_size > DATA_BLOCK_SIZE_THRESHOLD) {
        // 块满了 (超过 128 字节)，执行刷盘
        FlushDataBlock();
        // 重置“便签”，为新块记录起始位置
        cur_data_block_offset_ = static_cast<uint64_t>(ofs_.tellp());
    }

    // 3. 将 K/V 写入 *内存* 缓冲区 (函数来自 base.h)
    writeKV(&cur_data_block_, key, value);

    // 4. 实时更新“便签”上的“最后一个 Key”
    last_key_in_block_ = std::string(key); 
    
    return true;
}

/**
 * @brief (私有) 刷写数据块，并 *更新* 内存索引
 */
void SSTableBuilder::FlushDataBlock() {
    if (cur_data_block_.empty()) {
        return; // 没有数据可刷
    }

    // 1. 将数据块缓冲区写入文件
    ofs_.write(cur_data_block_.data(), cur_data_block_.size());
    std::cout << "  [Builder] 刷盘 Data Block (Last Key: " << last_key_in_block_ << ")" << std::endl;

    // 2. 创建 BlockHandle (指向刚写入的块)
    BlockHandle handle;
    handle.offset_ = cur_data_block_offset_; // 使用“便签”上的 offset
    handle.size_ = static_cast<uint32_t>(cur_data_block_.size());

    // 3. 【实现】将索引条目 (last_key, handle) 存入 *内存* 索引
    index_data_[last_key_in_block_] = handle;

    // 4. 重置 Data Block 缓冲区
    cur_data_block_.clear();
    // (注意: cur_data_block_offset_ 会在 Add() 的开头被重置)
}

/**
 * @brief (收尾) 写入 Index Block 和 Footer
 */
bool SSTableBuilder::Finish() {
    if (finished_ || !ofs_) return false;

    // 1. 刷盘最后一个 Data Block
    FlushDataBlock();

    // 2. 准备并写入 Index Block
    uint64_t index_block_offset = static_cast<uint64_t>(ofs_.tellp());
    std::string index_block_buffer; 
    std::cout << "  [Builder] 在 offset " << index_block_offset << " 写入索引块..." << std::endl;

    for (const auto& pair : index_data_) {
        // ... (写入 index_block_buffer) ...
        const std::string& last_key = pair.first;
        const BlockHandle& handle = pair.second;
        std::string handle_encoded; 
        handle.EncodeTo(&handle_encoded);
        writeKV(&index_block_buffer, last_key, handle_encoded);
    }
    
    ofs_.write(index_block_buffer.data(), index_block_buffer.size());
    
    // 3. 准备并写入 Footer
    Footer footer;
    footer.index_block_handle_.offset_ = index_block_offset; 
    footer.index_block_handle_.size_ = static_cast<uint32_t>(index_block_buffer.size()); 
    footer.magic_number_ = SSTABLE_MAGIC_NUMBER;

    std::string footer_encoded;
    footer.EncodeTo(&footer_encoded); 

    ofs_.write(footer_encoded.data(), FOOTER_SIZE);

    // --- 4. 收尾 ---

    // 【修复】必须在 close() *之前* 获取文件大小
    uint64_t final_file_size = static_cast<uint64_t>(ofs_.tellp());

    finished_ = true; 
    ofs_.close();      
    
    // 【修复】现在打印正确的大小
    std::cout << "--- SSTable 构建完成 (" << final_file_size << " 字节) ---" << std::endl; 
    return true;
}