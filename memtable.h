#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <string_view> // 用于 get() 和 ApproximateSize()

/**
 * @brief MemTable (内存表)
 * 职责：只在内存中缓冲有序的 K/V。
 * 它对磁盘、文件、SSTable 格式一无所知。
 */
class memtable {
public:
    memtable() = default;

    /**
     * @brief 向内存中插入/更新一个 K/V。
     * (使用 const& 避免不必要的字符串拷贝)
     */
    void put(const std::string& key, const std::string& value);

    /**
     * @brief 尝试从内存中获取一个 Key。
     * (LSMTree 的 Get() 会先查 MemTable)
     */
    bool get(std::string_view key, std::string* value) const;

    /**
     * @brief 为“刷盘”提供一个外部访问 map 的只读接口。
     */
    const std::map<std::string, std::string>& GetMap() const;

    /**
     * @brief 估算 MemTable 当前占用的内存大小。
     * (LSMTree 管理者用它来决定何时刷盘)
     */
    size_t ApproximateSize() const;

private:
    std::map<std::string, std::string> memtable_;
};
