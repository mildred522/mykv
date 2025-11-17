# 1. 设置 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.15)

# 2. 定义你的项目名称
# (这也会是 Visual Studio 等 IDE 中显示的项目名)
project(MyKVStore)

# 3. 设置 C++ 标准为 C++17
# (我们 V1 迭代中使用了 std::string_view, 需要 C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. 【核心】添加编译选项以解决中文乱码 (GBK)
# 告诉 g++ 编译器：
# -finput-charset=UTF-8: 你的 .cpp 源文件是 UTF-8 编码
# -fexec-charset=GBK:     请把程序中的字符串(cout)在编译时 *转换* 为 GBK 编码
# (这正是你需要的，且只对 g++/clang 有效。根据你的环境 (MinGW)，这完美匹配)
add_compile_options(-finput-charset=UTF-8)

# 5. (推荐) 开启调试信息和所有警告
# "-g" 允许你使用 GDB 调试
# "-Wall" 开启所有警告，帮你找到潜在 bug
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

# 6. 列出所有的 *实现* 文件 (.cpp)
# CMake 会自动处理 .h 文件的依赖关系
set(SOURCE_FILES
    memtable.cpp
    sstablebuilder.cpp
    sstablereader.cpp
    test.cpp  # 这就是你的 main() 函数所在的文件
)

# 7. 创建可执行文件
# 将所有 .cpp 源文件编译并链接成一个名为 "run_test" 的可执行文件
# (在 Windows 上会自动生成 "run_test.exe")
add_executable(run_test ${SOURCE_FILES})