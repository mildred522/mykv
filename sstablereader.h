#pragma once

#include <string>
#include <map>
#include <fstream>
#include <string_view>
#include "base.h" // 包含 BlockHandle, Footer, readKV, 和常量

/**
 * @brief SSTableReader (读取器)
 * 职责：只读取 SSTable。
 * 负责打开一个 SSTable, (倒着读)加载其索引, 并提供 Get() 方法。
 * 这是一个“持久”的类，在构造时加载索引。
 */
class SSTableReader {
public:
    /**
     * @brief 构造函数：打开一个文件准备读取
     * @param filename 要读取的 SSTable 文件名
     */
    explicit SSTableReader(const std::string& filename);

    /**
     * @brief 析构函数：关闭文件
     */
    ~SSTableReader();

    // 禁用拷贝和赋值 (防止意外的文件句柄拷贝)
    SSTableReader(const SSTableReader&) = delete;
    SSTableReader& operator=(const SSTableReader&) = delete;

    /**
     * @brief (核心 API) 查找一个 Key。
     * 执行“两级查找”（1. 查内存索引 -> 2. 查磁盘数据块）
     * @param key 要查找的 Key
     * @param value [out] 如果找到，值被存入这里
     * @return true 如果找到, false 如果未找到
     */
    bool Get(std::string_view key, std::string* value);

    /**
     * @brief 检查文件是否成功打开并且索引已加载
     */
    bool is_valid() const { return is_valid_; }

private:
    /**
     * @brief (私有) 在构造时调用，读取 Footer 和 Index Block 到内存
     * @return true 成功加载, false 失败
     */
    bool LoadIndex();

    /**
     * @brief (私有 I/O) 根据 BlockHandle 从磁盘读取一个 Data Block
     * @param handle 指向 Data Block 的指针 (offset, size)
     * @param block_content [out] 读出的数据块内容
     * @return true 成功, false 失败
     */
    bool ReadDataBlock(const BlockHandle& handle, std::string* block_content);

    /**
     * @brief (私有 CPU) 在内存中的 Data Block (buffer) 中查找 Key
     * @param block_content 包含 K/V 序列的内存缓冲区
     * @param key 要查找的 Key
     * @param value [out] 如果找到，值被存入这里
     * @return true 找到, false 未找到
     */
    bool FindInBlock(std::string_view block_content, std::string_view key, std::string* value);

    // --- 成员变量 (统一带 _ 后缀) ---
    
    std::ifstream ifs_; // 输入文件流
    Footer footer_;     // 文件的 Footer (在 LoadIndex 时填充)
    bool is_valid_;     // 标记文件是否成功打开和加载
    
    // 内存中的索引 (目录)
    // Key: last_key_in_block, Value: BlockHandle (指向 Data Block)
    std::map<std::string, BlockHandle> index_data_;
};