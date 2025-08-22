#include<parallel_algorithm/merge_sort.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm> // 用于std::sort对比
#include <random>
#include <string>
#include <functional> // 用于自定义比较

// 测试1：基本整数排序（与标准库sort对比）
TEST(MergeSortTest, BasicIntSort)
{
    std::vector<int> data = { 38, 27, 43, 3, 9, 82, 10 };
    std::vector<int> expected = data;
    std::sort(expected.begin(), expected.end()); // 标准库排序作为基准

    // 测试递归版本
    std::vector<int> recursive_data = data;
    merge_sort(recursive_data);
    EXPECT_EQ(recursive_data, expected);

    // 测试非递归版本
    std::vector<int> iterative_data = data;
    merge_sort_iterative(iterative_data);
    EXPECT_EQ(iterative_data, expected);
}

// 测试2：空数组（边界条件）
TEST(MergeSortTest, EmptyArray)
{
    std::vector<double> data;

    // 递归版本
    merge_sort(data);
    EXPECT_TRUE(data.empty());

    // 非递归版本
    merge_sort_iterative(data);
    EXPECT_TRUE(data.empty());
}

// 测试3：单元素数组（边界条件）
TEST(MergeSortTest, SingleElement)
{
    std::vector<char> data = { 'a' };
    std::vector<char> expected = data;

    merge_sort(data);
    EXPECT_EQ(data, expected);

    data = { 'a' }; // 重置
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

// 测试4：已排序数组（最好情况）
TEST(MergeSortTest, SortedArray)
{
    std::vector<int> data = { 1, 2, 3, 4, 5, 6 };
    std::vector<int> expected = data;

    merge_sort(data);
    EXPECT_EQ(data, expected);

    data = { 1, 2, 3, 4, 5, 6 };
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

// 测试5：逆序数组（最坏情况）
TEST(MergeSortTest, ReverseSortedArray)
{
    std::vector<int> data = { 6, 5, 4, 3, 2, 1 };
    std::vector<int> expected = { 1, 2, 3, 4, 5, 6 };

    merge_sort(data);
    EXPECT_EQ(data, expected);

    data = { 6, 5, 4, 3, 2, 1 };
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

// 测试6：包含重复元素的数组
TEST(MergeSortTest, DuplicateElements)
{
    std::vector<int> data = { 5, 3, 8, 3, 5, 1, 5 };
    std::vector<int> expected = { 1, 3, 3, 5, 5, 5, 8 };

    merge_sort(data);
    EXPECT_EQ(data, expected);

    data = { 5, 3, 8, 3, 5, 1, 5 };
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

// 测试7：大数据量排序（验证性能和正确性）
TEST(MergeSortTest, LargeData)
{
    const size_t n = 100'000; // 10万个元素
    std::vector<int> data(n);

    // 生成随机数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100000);
    for (auto& x : data)
    {
        x = dist(gen);
    }

    // 标准库排序作为基准
    std::vector<int> expected = data;
    std::sort(expected.begin(), expected.end());

    // 递归版本（注意：过大数据可能导致栈溢出，视编译器栈大小而定）
    std::vector<int> recursive_data = data;
    merge_sort(recursive_data);
    EXPECT_EQ(recursive_data, expected);

    // 非递归版本（适合大数据）
    std::vector<int> iterative_data = data;
    merge_sort_iterative(iterative_data);
    EXPECT_EQ(iterative_data, expected);

    // 打印性能参考（非断言，仅作信息）
    std::cout << "\nLargeData: 100,000 elements sorted successfully\n";
}

// 测试8：自定义类型排序（验证泛型支持）
TEST(MergeSortTest, CustomType)
{
    // 自定义结构体：包含值和原始索引（用于验证稳定性）
    struct MyType
    {
        int value;
        int index; // 原始位置，用于检查稳定性
        MyType(int v, int i) : value(v), index(i) {}
        bool operator<(const MyType& other) const
        {
            if(value != other.value)
                return value < other.value; // 按value排序
            else 
                return index < other.index; // 保持原始index顺序（稳定性）
        }

        bool operator==(const MyType& other) const
        {
            return value == other.value && index == other.index;
        }

        MyType() = default; // 默认构造函数
    };


    // 测试数据：包含重复value，记录原始index
    std::vector<MyType> data;
    data.emplace_back(3, 0);
    data.emplace_back(1, 1);
    data.emplace_back(3, 2); // 与第一个元素value相同
    data.emplace_back(2, 3);

    // 预期结果：value升序，且两个value=3的元素保持原始index顺序（0 < 2）
    std::vector<MyType> expected;
    expected.emplace_back(1, 1);
    expected.emplace_back(2, 3);
    expected.emplace_back(3, 0);
    expected.emplace_back(3, 2);

    // 对自定义类型使用归并排序（需确保merge函数中的比较符正确）
    // 注：当前merge函数使用<=，已保证稳定性
    merge_sort(data);
    EXPECT_EQ(data, expected);

    // 非递归版本同样验证
    data.clear();
    data.emplace_back(3, 0);
    data.emplace_back(1, 1);
    data.emplace_back(3, 2);
    data.emplace_back(2, 3);
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

// 测试9：字符串排序（验证泛型支持）
TEST(MergeSortTest, StringSort)
{
    std::vector<std::string> data = { "banana", "apple", "cherry", "date", "apple" };
    std::vector<std::string> expected = { "apple", "apple", "banana", "cherry", "date" };

    merge_sort(data);
    EXPECT_EQ(data, expected);

    data = { "banana", "apple", "cherry", "date", "apple" };
    merge_sort_iterative(data);
    EXPECT_EQ(data, expected);
}

