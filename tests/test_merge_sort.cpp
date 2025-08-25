#include<parallel_algorithm/merge_sort.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm> // 用于std::sort对比
#include <random>
#include <string>
#include <functional> // 用于自定义比较
#include <chrono>
#include <thread>
#include <atomic>

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



// 测试1：基础正确性（与std::sort对比）
TEST(ParallelMergeSortTest, BasicCorrectness)
{
    std::vector<int> data = { 38, 27, 43, 3, 9, 82, 10 };
    std::vector<int> expected = data;
    std::sort(expected.begin(), expected.end());

    // 测试默认参数（自动线程数）
    std::vector<int> parallel_data = data;
    parallel_merge_sort(parallel_data);
    EXPECT_EQ(parallel_data, expected);

    // 测试指定线程数（2线程）
    parallel_data = data;
    parallel_merge_sort(parallel_data, 1000, 2);
    EXPECT_EQ(parallel_data, expected);
}

// 测试2：边界条件（空数组、单元素、已排序、逆序）
TEST(ParallelMergeSortTest, EdgeCases)
{
    // 空数组
    std::vector<float> empty_data;
    parallel_merge_sort(empty_data);
    EXPECT_TRUE(empty_data.empty());

    // 单元素
    std::vector<std::string> single_data = { "test" };
    parallel_merge_sort(single_data);
    EXPECT_EQ(single_data, std::vector<std::string>{"test"});

    // 已排序数组
    std::vector<int> sorted_data = { 1, 2, 3, 4, 5 };
    std::vector<int> sorted_expected = sorted_data;
    parallel_merge_sort(sorted_data);
    EXPECT_EQ(sorted_data, sorted_expected);

    // 逆序数组
    std::vector<int> reverse_data = { 5, 4, 3, 2, 1 };
    std::vector<int> reverse_expected = { 1, 2, 3, 4, 5 };
    parallel_merge_sort(reverse_data);
    EXPECT_EQ(reverse_data, reverse_expected);
}

// 测试3：重复元素与稳定性（自定义类型）
TEST(ParallelMergeSortTest, StabilityWithDuplicates)
{
    struct StableType
    {
        int value;
        int index; // 原始索引，用于验证稳定性
        StableType(int v, int i) : value(v), index(i) {}
        bool operator<(const StableType& other) const
        {
            return value < other.value; // 仅按value排序
        }
        bool operator==(const StableType& other) const
        {
            return value == other.value && index == other.index;
        }
    };

    // 测试数据：包含重复value，记录原始index
    std::vector<StableType> data;
    data.emplace_back(3, 0);  // value=3，原始位置0
    data.emplace_back(1, 1);  // value=1，原始位置1
    data.emplace_back(3, 2);  // value=3，原始位置2（与第一个重复）
    data.emplace_back(2, 3);  // value=2，原始位置3

    // 预期结果：value升序，且重复value的元素保持原始index顺序
    std::vector<StableType> expected;
    expected.emplace_back(1, 1);
    expected.emplace_back(2, 3);
    expected.emplace_back(3, 0);  // 原始index 0 在 2 之前
    expected.emplace_back(3, 2);

    // 并行排序后验证
    parallel_merge_sort(data, 1000, 4); // 4线程
    EXPECT_EQ(data, expected);
}

// 测试4：线程数量控制（验证不会超过max_threads）
TEST(ParallelMergeSortTest, ThreadCountControl)
{
    // 思路：通过原子变量计数实际创建的线程数
    static std::atomic<size_t> thread_counter = 0;
    static size_t max_observed_threads = 0;

    // 自定义线程包装器：计数并监控最大线程数
    auto thread_wrapper = [&](auto&& func, auto&&... args) {
        thread_counter++;
        max_observed_threads = std::max(max_observed_threads, thread_counter.load());
        std::invoke(std::forward<decltype(func)>(func), std::forward<decltype(args)>(args)...);
        thread_counter--;
        };

    // 生成足够大的数据（确保触发并行）
    const size_t n = 100'000;
    std::vector<int> data(n);
    std::generate(data.begin(), data.end(), []() { return rand() % 1000; });

    // 测试1：限制最大线程数为2
    thread_counter = 0;
    max_observed_threads = 0;
    parallel_merge_sort(data, 1000, 2); // 最大2线程
    EXPECT_LE(max_observed_threads, 2) << "线程数超过限制（预期≤2）";

    // 测试2：限制最大线程数为4
    thread_counter = 0;
    max_observed_threads = 0;
    parallel_merge_sort(data, 1000, 4); // 最大4线程
    EXPECT_LE(max_observed_threads, 4) << "线程数超过限制（预期≤4）";
}

// 测试5：性能对比（并行vs串行，大数据量）
TEST(ParallelMergeSortTest, PerformanceComparison)
{
    const size_t n = 10'000'000; // 1000万随机整数
    std::vector<int> data(n);

    // 生成随机数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100'000);
    for (auto& x : data) x = dist(gen);

    // 并行排序（自动线程数）
    auto parallel_data = data;
    auto start_parallel = std::chrono::high_resolution_clock::now();
    parallel_merge_sort(parallel_data);
    auto time_parallel = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_parallel
    ).count();

    // 串行排序（std::sort作为基准）
    auto serial_data = data;
    auto start_serial = std::chrono::high_resolution_clock::now();
    std::sort(serial_data.begin(), serial_data.end());
    auto time_serial = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_serial
    ).count();

    // 验证结果一致
    EXPECT_EQ(parallel_data, serial_data);

    // 打印性能数据（非断言，仅作参考）
    std::cout << "\n性能对比（" << n << "元素）:\n";
    std::cout << "并行排序时间: " << time_parallel << "ms\n";
    std::cout << "串行排序时间: " << time_serial << "ms\n";
    std::cout << "加速比: " << static_cast<double>(time_serial) / time_parallel << "x\n";

    // 粗略验证并行有加速（至少1.2x，根据硬件可能调整）
    EXPECT_GT(static_cast<double>(time_serial) / time_parallel, 1.2)
        << "并行排序未体现足够加速比";
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

