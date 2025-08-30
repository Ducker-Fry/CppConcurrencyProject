# 最低 CMake 版本要求
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_TOOLCHAIN_FILE "E:/Software/Vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING  "Vcpkg toolchain file")

project ("CppConcurrencyProject")
set(CMAKE_CXX_STANDARD 20)

# 查找 vcpkg 安装的 GTest，明确指定组件
find_package(GTest REQUIRED)

# 包含目录配置
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/include/thread_safe_queue/")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/include/matrix")


# 查找主程序源文件
file(GLOB MAIN_SOURCES "src/*.cpp")
file(GLOB MAIN_HEADERS "include/*.h" "include/*/*.h")

# 查找测试源文件
file(GLOB TEST_SOURCES "tests/*.cpp")

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# 生成主程序可执行文件
add_executable(app main.cpp ${MAIN_SOURCES} ${MAIN_HEADERS})

# 生成测试程序可执行文件
add_executable(test_app ${TEST_SOURCES})
target_link_libraries(test_app
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET app PROPERTY CXX_STANDARD 20)
  set_property(TARGET test_app PROPERTY CXX_STANDARD 20)
endif()

# 启用测试
enable_testing()
# 引入 GoogleTest 提供的测试发现相关功能
include(GoogleTest)
# 注册测试用例，让 CMake 能发现测试
gtest_discover_tests(test_app)
