#include <parallel_algorithm/prefix_sum.h>
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <climits>
#include <chrono>

// 测试1：基础加法前缀和（int类型）
TEST(GenericPrefixSumTest, BasicAdditionInt)
{
    std::vector<int> arr = { 1, 2, 3, 4, 5 };
    std::vector<int> expected = { 0, 1, 3, 6, 10, 15 };

    auto result = compute_prefix(arr, PrefixOps::add<int>(), 0);
    EXPECT_EQ(result, expected);
}

// 测试2：乘法前缀积（double类型）
TEST(GenericPrefixSumTest, MultiplicationDouble)
{
    std::vector<double> arr = { 1.5, 2.0, 3.0, 4.0 };
    std::vector<double> expected = { 1.0, 1.5, 3.0, 9.0, 36.0 };

    auto result = compute_prefix(arr, PrefixOps::multiply<double>(), 1.0);
    EXPECT_EQ(result, expected);
}

// 测试3：前缀最小值（int类型）
TEST(GenericPrefixSumTest, MinPrefix)
{
    std::vector<int> arr = { 5, 3, 7, 2, 8 };
    std::vector<int> expected = { INT_MAX, 5, 3, 3, 2, 2 }; // 初始值用INT_MAX

    auto result = compute_prefix(arr, PrefixOps::min<int>(), INT_MAX);
    EXPECT_EQ(result, expected);
}

// 测试4：前缀最大值（long类型）
TEST(GenericPrefixSumTest, MaxPrefix)
{
    std::vector<long> arr = { 10, 5, 20, 15, 25 };
    std::vector<long> expected = { LONG_MIN, 10, 10, 20, 20, 25 }; // 初始值用LONG_MIN

    auto result = compute_prefix(arr, PrefixOps::max<long>(), LONG_MIN);
    EXPECT_EQ(result, expected);
}

// 测试5：自定义操作（字符串拼接）
TEST(GenericPrefixSumTest, CustomOperationString)
{
    std::vector<std::string> arr = { "Hello", " ", "World", "!" };
    std::vector<std::string> expected = { "", "Hello", "Hello ", "Hello World", "Hello World!" };

    auto concat_op = [](const std::string& a, const std::string& b) {
        return a + b;
        };
    auto result = compute_prefix<std::string>(arr, concat_op, std::string(""));
    EXPECT_EQ(result, expected);
}

// 测试6：空数组（边界条件）
TEST(GenericPrefixSumTest, EmptyArray)
{
    std::vector<float> arr;
    std::vector<float> expected = { 0.0f }; // 仅包含单位元

    auto result = compute_prefix(arr, PrefixOps::add<float>(), 0.0f);
    EXPECT_EQ(result, expected);
}

// 测试7：单元素数组（边界条件）
TEST(GenericPrefixSumTest, SingleElement)
{
    std::vector<char> arr = { 'a' };
    std::vector<char> expected = { '\0', 'a' }; // 字符加法（实际是ASCII码相加）

    auto result = compute_prefix(arr, PrefixOps::add<char>(), '\0');
    EXPECT_EQ(result, expected);
}

// 测试8：自定义类型（结构体）
TEST(GenericPrefixSumTest, CustomType)
{
    // 自定义坐标类型
    struct Point
    {
        int x;
        int y;
        Point(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}

        // 重载==用于EXPECT_EQ比较
        bool operator==(const Point& other) const
        {
            return x == other.x && y == other.y;
        }
    };

    // 自定义操作：点的累加（x1+x2, y1+y2）
    auto point_add = [](const Point& a, const Point& b) {
        return Point(a.x + b.x, a.y + b.y);
        };

    // 测试数据
    std::vector<Point> arr = { Point(1,2), Point(3,4), Point(5,6) };
    std::vector<Point> expected = {
        Point(0,0),          // 单位元
        Point(1,2),          // 第一个点
        Point(4,6),          // (1+3, 2+4)
        Point(9,12)          // (4+5, 6+6)
    };

    auto result = compute_prefix<Point>(arr, point_add, Point(0, 0));
    EXPECT_EQ(result, expected);
}

// 测试9：异常情况（空操作符）
TEST(GenericPrefixSumTest, NullOperation)
{
    std::vector<int> arr = { 1, 2, 3 };
    std::function<int(const int&, const int&)> null_op = nullptr;

    // 预期抛出异常
    EXPECT_THROW(
        compute_prefix(arr, null_op, 0),
        std::invalid_argument
    );
}


// Test 1: Basic addition (integer type)
TEST(ParallelPrefixTest, BasicAddition) {
    std::vector<int> arr = {1, 2, 3, 4, 5};
    auto op = [](int a, int b) { return a + b; };
    const int identity = 0;

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 2: Floating-point addition
TEST(ParallelPrefixTest, FloatAddition) {
    std::vector<float> arr = {1.5f, 2.5f, 3.5f, 4.5f};
    auto op = [](float a, float b) { return a + b; };
    const float identity = 0.0f;

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    for (size_t i = 0; i < parallel_result.size(); ++i) {
        EXPECT_NEAR(parallel_result[i], sequential_result[i], 1e-6f);
    }
}

// Test 3: Multiplication operation
TEST(ParallelPrefixTest, Multiplication) {
    std::vector<int> arr = {2, 3, 4, 5};
    auto op = [](int a, int b) { return a * b; };
    const int identity = 1;

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 4: String concatenation
TEST(ParallelPrefixTest, StringConcatenation) {
    std::vector<std::string> arr = {"a", "b", "c", "d"};
    auto op = [](const std::string& a, const std::string& b) { return a + b; };
    const std::string identity = "";

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 5: Maximum value operation
TEST(ParallelPrefixTest, MaxOperation) {
    std::vector<int> arr = {3, 1, 4, 2, 5};
    auto op = [](int a, int b) { return std::max(a, b); };
    const int identity = INT_MIN;

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 6: Empty array (edge case)
TEST(ParallelPrefixTest, EmptyArray) {
    std::vector<double> arr;
    auto op = [](double a, double b) { return a + b; };
    const double identity = 0.0;

    auto parallel_result = parallel_prefix(arr, op, identity);
    EXPECT_EQ(parallel_result, std::vector<double>{identity});
}

// Test 7: Single element array (edge case)
TEST(ParallelPrefixTest, SingleElement) {
    std::vector<char> arr = {'x'};
    auto op = [](char a, char b) { return a + b; };
    const char identity = '\0';

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 8: Small array (fewer elements than threads)
TEST(ParallelPrefixTest, SmallArray) {
    std::vector<int> arr = {10, 20}; // Assuming 4+ threads available
    auto op = [](int a, int b) { return a + b; };
    const int identity = 0;

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 9: Custom type (point coordinates)
TEST(ParallelPrefixTest, CustomType) {
    struct Point {
        int x;
        int y;
        Point(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
        bool operator==(const Point& other) const {
            return x == other.x && y == other.y;
        }
    };

    std::vector<Point> arr = {Point(1,2), Point(3,4), Point(5,6)};
    auto op = [](const Point& a, const Point& b) {
        return Point(a.x + b.x, a.y + b.y);
    };
    const Point identity(0, 0);

    auto parallel_result = parallel_prefix(arr, op, identity);
    auto sequential_result = sequential_prefix(arr, op, identity);

    EXPECT_EQ(parallel_result, sequential_result);
}

// Test 10: Performance comparison (large dataset)
TEST(ParallelPrefixTest, PerformanceComparison) {
    const size_t n = 10'000'000;
    std::vector<long long> arr(n);
    for (size_t i = 0; i < n; ++i) {
        arr[i] = i % 100;
    }

    auto op = [](long long a, long long b) { return a + b; };
    const long long identity = 0;

    // Parallel version
    auto start_parallel = std::chrono::high_resolution_clock::now();
    auto parallel_result = parallel_prefix(arr, op, identity);
    auto time_parallel = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_parallel
    ).count();

    // Sequential version
    auto start_sequential = std::chrono::high_resolution_clock::now();
    auto sequential_result = sequential_prefix(arr, op, identity);
    auto time_sequential = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_sequential
    ).count();

    // Verify correctness
    EXPECT_EQ(parallel_result, sequential_result);

    // Output performance data
    std::cout << "\nPerformance comparison (" << n << " elements):\n";
    std::cout << "Parallel time: " << time_parallel << "ms\n";
    std::cout << "Sequential time: " << time_sequential << "ms\n";
    std::cout << "Speedup: " << static_cast<double>(time_sequential) / time_parallel << "x\n";
}

