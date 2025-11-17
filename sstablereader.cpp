#include "sstablereader.h"
#include <iostream>
#include <vector>

/**
 * @brief 构造函数：打开文件并立即加载索引
 */
SSTableReader::SSTableReader(const std::string& filename)
    : ifs_(filename, std::ios::binary | std::ios::ate), // ate: 打开并定位到末尾
      is_valid_(false) { // 默认无效，直到 LoadIndex 成功
    
    if (!ifs_) {
        std::cerr << "错误: SSTableReader 无法打开文件 " << filename << std::endl;
        return;
    }
    
    // 构造时立即加载索引
    if (!LoadIndex()) {
        std::cerr << "错误: 无法加载索引 " << filename << std::endl;
        ifs_.close();
    } else {
        is_valid_ = true; // 加载成功
    }
}

/**
 * @brief 析构函数：关闭文件
 */
SSTableReader::~SSTableReader() {
    if (ifs_.is_open()) {
        ifs_.close();
    }
}

/**
 * @brief (私有) 在构造时调用，读取 Footer 和 Index Block
 */
bool SSTableReader::LoadIndex() {
    // 1. 获取文件大小
    std::streampos file_size = ifs_.tellg();
    if (file_size < FOOTER_SIZE) {
        std::cerr << "错误: 文件太小，不是有效的 SSTable" << std::endl;
        return false;
    }

    // 2. 读取 Footer (倒着读)
    std::string footer_buf;
    footer_buf.resize(FOOTER_SIZE);
    ifs_.seekg(static_cast<std::streamoff>(file_size) - FOOTER_SIZE); // 定位到 Footer
    ifs_.read(&footer_buf[0], FOOTER_SIZE);

    if (ifs_.gcount() != FOOTER_SIZE) {
        std::cerr << "错误: 读取 Footer 失败" << std::endl;
        return false;
    }

    // (DecodeFrom 来自 base.h，它会校验魔数)
    if (!footer_.DecodeFrom(footer_buf)) {
        std::cerr << "错误: 魔数不匹配或文件损坏" << std::endl;
        return false;
    }
    std::cout << "  [Reader] Footer 校验成功 (Magic Number OK)" << std::endl;

    // 3. 读取 Index Block (根据 Footer 的指引)
    std::string index_block_content;
    // (调用私有辅助函数 ReadDataBlock 来读取索引块)
    if (!ReadDataBlock(footer_.index_block_handle_, &index_block_content)) {
        std::cerr << "错误: 无法读取 Index Block" << std::endl;
        return false;
    }
    
    // 4. 解析 Index Block, 填充 index_data_ (内存中的 map)
    std::string_view input = index_block_content;
    while (!input.empty()) {
        std::string_view last_key;
        std::string_view handle_data;
        // (readKV 来自 base.h)
        if (!readKV(&input, &last_key, &handle_data)) {
            std::cerr << "错误: 解析 Index Block 失败" << std::endl;
            return false;
        }

        BlockHandle handle;
        // (DecodeFrom 来自 base.h)
        if (!handle.DecodeFrom(&handle_data)) {
            std::cerr << "错误: 解析 BlockHandle 失败" << std::endl;
            return false;
        }
        
        // 将 (last_key, handle) 存入内存 map
        index_data_[std::string(last_key)] = handle;
    }
    std::cout << "  [Reader] 索引加载完成, " << index_data_.size() << " 个条目。" << std::endl;
    return true;
}

/**
 * @brief (公有) 查找一个 Key
 */
bool SSTableReader::Get(std::string_view key, std::string* value) {
    if (!is_valid_) {
        return false; // 文件未成功加载
    }

    // --- 核心的两级查找 ---

    // 1.【查找级别 1 (内存)】: 在 Index Block (内存 map) 中二分查找
    // lower_bound: 找到第一个 *不小于* key 的条目。
    // 这就是 key *可能* 所在的那个 Data Block (的索引)。
    auto it = index_data_.lower_bound(std::string(key));
    if (it == index_data_.end()) {
        // key 比所有 Data Block 的 'last_key' 都大，所以不存在
        return false;
    }
    
    // 2. 找到了 Data Block 的句柄 (Handle)
    const BlockHandle& handle = it->second;

    // 3.【查找级别 2 (磁盘 I/O)】: 读取 Data Block 到内存
    std::string block_content;
    if (!ReadDataBlock(handle, &block_content)) {
        return false; // I/O 错误
    }

    // 4.【查找级别 3 (CPU)】: 在 Data Block 内部查找 Key
    return FindInBlock(block_content, key, value);
}

/**
 * @brief (私有 I/O) 根据 BlockHandle 读取一个完整的块到内存
 */
bool SSTableReader::ReadDataBlock(const BlockHandle& handle, std::string* block_content) {
    block_content->resize(handle.size_);
    ifs_.seekg(handle.offset_); // 定位
    ifs_.read(&(*block_content)[0], handle.size_); // 读取
    
    if (ifs_.gcount() != handle.size_) {
        std::cerr << "错误: 读取 Data Block 失败 (预期 " << handle.size_ 
                  << ", 实际 " << ifs_.gcount() << ")" << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief (私有 CPU) 在内存块中线性扫描
 * (V1 实现：线性扫描。V2 可升级为二分查找)
 */
bool SSTableReader::FindInBlock(std::string_view block_content, std::string_view key, std::string* value) {
    std::string_view input = block_content;
    while (!input.empty()) {
        std::string_view current_key;
        std::string_view current_value;
        // (readKV 来自 base.h)
        if (!readKV(&input, &current_key, &current_value)) {
            return false; // 块损坏
        }
        
        if (current_key == key) {
            *value = std::string(current_value);
            return true; // 找到了！
        }
        if (current_key > key) {
            // 优化：Data Block 内部也是有序的，如果
            // 当前 key 已经大于要找的 key，说明找不到了
            return false;
        }
    }
    return false; // 块内未找到
}
