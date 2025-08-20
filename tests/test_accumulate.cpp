#include<parallel_algorithm/accumulate.h>
#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <string>
#include <functional> // 用于std::multiplies等
#include <numeric>    // 用于与标准库版本对比

// 测试1：基础求和（使用默认加法操作）
TEST(MyAccumulateTest, BasicSum)
{
    // 测试vector容器
    std::vector<int> vec = { 1, 2, 3, 4, 5 };
    EXPECT_EQ(my_accumulate(vec.begin(), vec.end(), 0), 15);
    EXPECT_EQ(my_accumulate(vec.begin(), vec.end(), 10), 25); // 初始值非0

    // 测试list容器
    std::list<double> lst = { 1.5, 2.5, 3.5 };
    EXPECT_EQ(my_accumulate(lst.begin(), lst.end(), 0.0), 7.5);
    EXPECT_EQ(my_accumulate(lst.begin(), lst.end(), 2.5), 10.0);
}

// 测试2：自定义二元操作（乘法、减法等）
TEST(MyAccumulateTest, CustomBinaryOp)
{
    std::vector<int> nums = { 2, 3, 4 };

    // 测试乘法（求乘积）
    int product = my_accumulate(nums.begin(), nums.end(), 1, std::multiplies<int>());
    EXPECT_EQ(product, 24); // 1 * 2 * 3 * 4 = 24

    // 测试减法（从初始值开始连续减元素）
    int diff = my_accumulate(nums.begin(), nums.end(), 10, std::minus<int>());
    EXPECT_EQ(diff, 1); // 10 - 2 - 3 - 4 = 1

    // 测试取最大值（自定义lambda）
    int max_val = my_accumulate(nums.begin(), nums.end(), 0,
        [](int current_max, int val) {
            return std::max(current_max, val);
        });
    EXPECT_EQ(max_val, 4);
}

// 测试3：字符串拼接（自定义操作）
TEST(MyAccumulateTest, StringConcatenation)
{
    std::vector<std::string> parts = { "Hello", " ", "C++", " ", "World" };

    // 拼接字符串
    std::string result = my_accumulate(parts.begin(), parts.end(), std::string(),
        [](const std::string& a, const std::string& b) {
            return a + b;
        });
    EXPECT_EQ(result, "Hello C++ World");

    // 带初始字符串的拼接
    std::string prefixed = my_accumulate(parts.begin(), parts.end(), std::string("Prefix: "),
        [](const std::string& a, const std::string& b) {
            return a + b;
        });
    EXPECT_EQ(prefixed, "Prefix: Hello C++ World");
}

// 测试4：处理自定义类型
TEST(MyAccumulateTest, CustomType)
{
    // 定义一个简单的结构体：统计总和与元素个数
    struct Aggregate
    {
        int sum = 0;
        size_t count = 0;
    };

    std::vector<int> data = { 3, 1, 4, 1, 5 };

    // 累积计算总和与个数
    Aggregate agg = my_accumulate(data.begin(), data.end(), Aggregate(),
        [](Aggregate a, int val) {
            a.sum += val;
            a.count++;
            return a;
        });

    EXPECT_EQ(agg.sum, 14);    // 3+1+4+1+5=14
    EXPECT_EQ(agg.count, 5);   // 共5个元素
}

// 测试5：边界条件（空范围）
TEST(MyAccumulateTest, EmptyRange)
{
    std::vector<int> empty_vec;

    // 空范围时，返回初始值
    EXPECT_EQ(my_accumulate(empty_vec.begin(), empty_vec.end(), 0), 0);
    EXPECT_EQ(my_accumulate(empty_vec.begin(), empty_vec.end(), 100), 100);
    EXPECT_EQ(my_accumulate(empty_vec.begin(), empty_vec.end(), std::string("test")), "test");
}

// 测试6：单元素范围
TEST(MyAccumulateTest, SingleElement)
{
    std::vector<char> single_char = { 'a' };
    // 修复1：将lambda的返回值显式转换为int（或保持自动推导，明确类型）
    // 修复2：预期结果用int类型表示（162），避免char类型的隐式截断
    EXPECT_EQ(my_accumulate(single_char.begin(), single_char.end(), 65,
        [](char a, char b) -> int {
            // 显式转换为int，明确返回类型
            return std::toupper(a) +
                std::tolower(b);
        }),
        162); // 'A'(65) + 'a'(97) = 162（直接用int值更清晰）
    // 输入65不是‘A’，因为会推导成char类型，而返回的162超过了char的范围-127~128.

    std::list<int> single_int = { 5 };
    EXPECT_EQ(my_accumulate(single_int.begin(), single_int.end(), 10), 15);
}

// 测试7：与标准库std::accumulate对比（确保行为一致）
TEST(MyAccumulateTest, CompareWithStdAccumulate)
{
    std::vector<double> data = { 1.1, 2.2, 3.3, 4.4 };

    // 对比默认加法
    EXPECT_EQ(my_accumulate(data.begin(), data.end(), 0.0),
        std::accumulate(data.begin(), data.end(), 0.0));

    // 对比自定义乘法
    EXPECT_EQ(my_accumulate(data.begin(), data.end(), 1.0, std::multiplies<double>()),
        std::accumulate(data.begin(), data.end(), 1.0, std::multiplies<double>()));
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
