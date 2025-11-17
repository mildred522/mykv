#pragma once

#include <string>
#include <map>
#include <fstream>      // 包含 std::ofstream
#include <string_view>  // 包含 std::string_view
#include "base.h"       // 包含 BlockHandle, Footer, getEntrySize, writeKV, 和常量

/**
 * @brief SSTableBuilder (构建器)
 * 负责按顺序写入 K/V，并生成 V2 格式的 SSTable 文件。
 * 这是一个“一次性”的类，在 Finish() 后失效。
 */
class SSTableBuilder {
public:
    /**
     * @brief 构造函数：打开一个文件准备写入
     * @param filename 要创建的 SSTable 文件名
     */
    explicit SSTableBuilder(const std::string& filename);

    /**
     * @brief 析构函数：确保文件被关闭
     */
    ~SSTableBuilder();

    // 禁用拷贝和赋值 (防止意外的文件句柄拷贝)
    SSTableBuilder(const SSTableBuilder&) = delete;
    SSTableBuilder& operator=(const SSTableBuilder&) = delete;

    /**
     * @brief 添加一个键值对到 SSTable。
     * K/V 会被缓冲，直到数据块 (Data Block) 满了（128字节）才刷盘。
     * @note 必须按 Key 升序调用！
     * @param key 键
     * @param value 值
     * @return true 成功；false 如果状态错误 (如已 Finish)
     */
    bool Add(std::string_view key, std::string_view value);

    /**
     * @brief 完成 SSTable 的构建。
     * 1. 刷盘最后一个 Data Block。
     * 2. 写入 Index Block。
     * 3. 写入 Footer。
     * 4. 关闭文件。
     * @return true 成功；false 如果状态错误
     */
    bool Finish();

    /**
     * @brief 检查文件是否成功打开
     */
    bool is_open() const { return ofs_.is_open(); }

private:
    /**
     * @brief (私有) 将当前内存中的 Data Block 刷入磁盘
     * 并在内存中更新索引 (index_data_)
     */
    void FlushDataBlock();

    // --- 成员变量 (统一带 _ 后缀) ---
    
    // 磁盘 I/O 相关
    std::ofstream ofs_;      // 输出文件流
    bool finished_;          // 是否已调用 Finish()
    
    // Data Block 相关
    std::string cur_data_block_;         // 当前数据块的内存缓冲区 (使用 std::string 作为缓冲区)
    std::string last_key_in_block_;      // 当前数据块的最后一个 Key (用于更新索引)
    uint64_t cur_data_block_offset_;     // 当前数据块在文件中的起始偏移量
    
    // Index Block 相关
    // 内存中的“索引” (Key: last_key, Value: BlockHandle)
    std::map<std::string, BlockHandle> index_data_;
};