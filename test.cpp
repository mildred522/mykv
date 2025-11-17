#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cassert> // 用于 assert
#include "sstablebuilder.h"
#include "sstablereader.h"
// (base.h 已经被 builder/reader include 了)

/**
 * @brief (测试辅助) 验证 Get(key) 是否返回预期值
 */
void test_get(SSTableReader& reader, const std::string& key, const std::string& expected_value) {
    std::string value;
    bool found = reader.Get(key, &value);
    
    std::cout << "  - 正在测试 Get(" << key << ")..." << std::endl;
    if (found && value == expected_value) {
        std::cout << "    > PASSED: Get(" << key << ") == " << value << std::endl;
    } else if (found) {
        std::cout << "    > FAILED: 预期 " << expected_value << ", 实际 " << value << std::endl;
    } else {
        std::cout << "    > FAILED: 预期 " << expected_value << ", 实际 Not Found" << std::endl;
    }
    // 断言，如果失败则程序终止
    assert(found && value == expected_value);
}

/**
 * @brief (测试辅助) 验证 Get(key) 是否 *未* 找到
 */
void test_get_notfound(SSTableReader& reader, const std::string& key) {
    std::string value;
    bool found = reader.Get(key, &value);

    std::cout << "  - 正在测试 Get(" << key << ") (预期 Not Found)..." << std::endl;
    if (!found) {
        std::cout << "    > PASSED: Get(" << key << ") 未找到." << std::endl;
    } else {
        std::cout << "    > FAILED: 预期 Not Found, 实际 " << value << std::endl;
    }
    // 断言
    assert(!found);
}


int main() {
    const std::string sst_filename = "test_v1.sst";
    
    // --- Phase 1: 构建 SSTable (SSTableBuilder Test) ---
    std::cout << "--- Phase 1: 正在构建 SSTable ---" << std::endl;
    { 
        SSTableBuilder builder(sst_filename);
        assert(builder.is_open()); // 断言文件已成功打开

        // 准备一个 *有序的* map，作为 MemTable 的模拟
        // (我们的阈值 DATA_BLOCK_SIZE_THRESHOLD = 128 字节)
        std::map<std::string, std::string> test_data = {
            {"s01_David", "88"}, {"s02_Bob", "82"}, {"s03_Alice", "95"},
            {"s04_Frank", "70"}, {"s05_Ivy", "92"}, {"s06_Eve", "85"},
            {"s07_Grace", "100"}, {"s08_Heidi", "78"}, {"s09_Charlie", "76"},
            {"s10_Jack", "89"}, {"s11_Kate", "91"}, {"s12_Liam", "77"},
            {"s13_Mia", "83"}, {"s14_Noah", "90"}, {"s15_Olivia", "99"}
        };
        
        // SSTableBuilder::Add 必须按顺序调用
        // std::map 的遍历自动保证了这一点
        for (const auto& pair : test_data) {
            builder.Add(pair.first, pair.second);
        }
        
        builder.Finish();
    } // builder 在这里析构，文件关闭

    std::cout << "\n--- Phase 2: 读取 SSTable (SSTableReader Test) ---" << std::endl;
    
    // --- Phase 2: 读取 SSTable ---
    SSTableReader reader(sst_filename);
    assert(reader.is_valid()); // 断言 Reader 成功加载了索引

    std::cout << "\n--- Phase 3: 验证 Builder 写入的数据 ---" << std::endl;

    // --- 测试 Get ---

    // 测试 1: 查找第一个 Block 的第一个 Key
    test_get(reader, "s01_David", "88");

    // 测试 2: 查找第一个 Block 的最后一个 Key
    test_get(reader, "s03_Alice", "95");

    // 测试 3: 查找中间 Block 的 Key
    test_get(reader, "s08_Heidi", "78");

    // 测试 4: 查找最后一个 Block 的最后一个 Key
    test_get(reader, "s15_Olivia", "99");

    // 测试 5: 查找一个不存在的 Key (在 Key 之间)
    test_get_notfound(reader, "s01_MiddleKey");

    // 测试 6: 查找一个不存在的 Key (比所有 Key 都大)
    test_get_notfound(reader, "zzz_Nobody");

    // 测试 7: 查找一个不存在的 Key (比所有 Key 都小)
    test_get_notfound(reader, "aaa_Nobody");

    std::cout << "\n--- V1 模块集成测试完成 ---" << std::endl;

    return 0;
}