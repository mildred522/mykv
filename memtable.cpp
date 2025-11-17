#include "memtable.h"
#include <iostream> // 用于打印

/**
 * @brief 向内存中插入/更新一个 K/V。
 */
void memtable::put(const std::string& key, const std::string& value) {
    std::cout << "[MemTable] 写入: (" << key << ", " << value << ")" << std::endl;
    memtable_[key] = value;
}

/**
 * @brief 尝试从内存中获取一个 Key。
 */
bool memtable::get(std::string_view key, std::string* value) const {
    // std::map::find 在 C++14 后可以接受 string_view
    // 为安全起见 (兼容 C++11)，我们先转为 string
    auto it = memtable_.find(std::string(key));
    if (it != memtable_.end()) {
        *value = it->second;
        return true;
    }
    return false;
}

/**
 * @brief 为“刷盘”提供一个外部访问 map 的只读接口。
 */
const std::map<std::string, std::string>& memtable::GetMap() const {
    return memtable_;
}

/**
 * @brief 估算 MemTable 当前占用的内存大小。
 * 这是一个粗略的估算，只计算 K/V 字符串在堆上的内存。
 * 真实的 LevelDB 会使用更精确的内存分配器来追踪。
 */
size_t memtable::ApproximateSize() const {
    size_t total_size = 0;
    
    // 加上 map 节点本身的开销 (估算)
    // (假设每个 map 节点 ≈ 48-64 字节)
    total_size += memtable_.size() * 64; 

    // 加上 K/V 字符串数据占用的堆内存 (使用 capacity() 更准)
    for (const auto& pair : memtable_) {
        total_size += pair.first.capacity();
        total_size += pair.second.capacity();
    }
    return total_size;
}
