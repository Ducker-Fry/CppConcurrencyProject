#include<parallel_algorithm/for_each.h>
#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <string>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <chrono>
#include <algorithm>

// 测试1：基础功能 - 并行遍历并修改元素（无共享资源）
TEST(ConcurrentForEachTest, BasicParallelModification) {
    const size_t n = 10000;
    std::vector<int> data(n);

    // 初始化：data[i] = i
    for (size_t i = 0; i < n; ++i) {
        data[i] = i;
    }

    // 并发遍历：每个元素乘以2（无共享资源，线程安全）
    parallel_for_each_s(data.begin(), data.end(), [](int& x) {
        x *= 2;
    });

    // 验证结果
    for (size_t i = 0; i < n; ++i) {
        EXPECT_EQ(data[i], 2 * static_cast<int>(i));
    }
}

// 测试2：线程安全 - 并行操作共享资源（需同步）
TEST(ConcurrentForEachTest, ThreadSafeSharedResource) {
    const size_t n = 5000;
    std::vector<int> data(n, 1); // 初始化为1
    std::atomic<int> sum(0);     // 原子变量用于共享累加（线程安全）

    // 并发遍历：累加所有元素（使用原子变量确保线程安全）
    parallel_for_each_s(data.begin(), data.end(), [&sum](int x) {
        sum += x;
    });

    // 预期结果：5000个1相加 = 5000
    EXPECT_EQ(sum, n);
}

// 测试3：线程安全 - 使用互斥锁保护共享资源
TEST(ConcurrentForEachTest, MutexProtectedSharedResource) {
    const size_t n = 3000;
    std::list<std::string> data(n, "a");
    std::string result;
    std::mutex mtx; // 用于保护result的互斥锁

    // 并发遍历：拼接字符串（通过互斥锁保证线程安全）
    parallel_for_each_s(data.begin(), data.end(), [&](const std::string& s) {
        std::lock_guard<std::mutex> lock(mtx);
        result += s;
    });

    // 预期结果：3000个"a"拼接
    EXPECT_EQ(result.size(), n);
    EXPECT_EQ(result, std::string(n, 'a'));
}

// 测试4：边界条件 - 空范围
TEST(ConcurrentForEachTest, EmptyRange) {
    std::vector<double> empty_data;
    bool called = false;

    // 并发遍历空范围
    parallel_for_each_s(empty_data.begin(), empty_data.end(), [&called](double) {
        called = true;
    });

    // 验证函数未被调用
    EXPECT_FALSE(called);
}

// 测试5：边界条件 - 元素数量小于线程数
TEST(ConcurrentForEachTest, SmallRange) {
    const size_t n = 3; // 元素数少于硬件线程数（通常≥4）
    std::vector<int> data(n);

    // 并发遍历：初始化元素
    parallel_for_each_s(data.begin(), data.end(), [](int& x) {
        static std::atomic<int> counter(0);
        x = counter++;
    });

    // 验证元素被正确初始化（顺序可能不连续，但值唯一）
    std::sort(data.begin(), data.end());
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 1);
    EXPECT_EQ(data[2], 2);
}

// 测试6：异常处理 - 子线程抛出异常
TEST(ConcurrentForEachTest, ExceptionPropagation) {
    std::vector<int> data(100);
    const std::string error_msg = "Parallel for_each failed: test exception";

    // 并发遍历：第50个元素触发异常
    EXPECT_THROW({
        parallel_for_each_s(data.begin(), data.end(), [&](int& x) {
            static std::atomic<int> counter(0);
            if (counter++ == 50) {
                throw std::runtime_error(error_msg);
            }
        });
    }, std::runtime_error);

    // 验证异常信息正确
    try {
        parallel_for_each_s(data.begin(), data.end(), [&](int& x) {
            static std::atomic<int> counter(0);
            if (counter++ == 50) {
                throw std::runtime_error(error_msg);
            }
        });
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(e.what(), error_msg);
    }
}

// 测试7：性能对比 - 与串行for_each的耗时比较（非严格测试）
TEST(ConcurrentForEachTest, PerformanceComparison) {
    const size_t n = 1000000; // 大数据量，便于观察并行优势
    std::vector<long long> data(n, 1);

    // 串行遍历耗时
    auto start_serial = std::chrono::high_resolution_clock::now();
    std::for_each(data.begin(), data.end(), [](long long& x) {
        x *= 2; // 简单计算，模拟耗时操作
    });
    auto end_serial = std::chrono::high_resolution_clock::now();
    auto time_serial = std::chrono::duration_cast<std::chrono::milliseconds>(end_serial - start_serial).count();

    // 重置数据
    std::fill(data.begin(), data.end(), 1);

    // 并发遍历耗时
    auto start_concurrent = std::chrono::high_resolution_clock::now();
    parallel_for_each_s(data.begin(), data.end(), [](long long& x) {
        x *= 2;
    });
    auto end_concurrent = std::chrono::high_resolution_clock::now();
    auto time_concurrent = std::chrono::duration_cast<std::chrono::milliseconds>(end_concurrent - start_concurrent).count();

    // 打印耗时（并行应快于串行，具体倍数取决于CPU核心数）
    std::cout << "Serial time: " << time_serial << "ms\n";
    std::cout << "Concurrent time: " << time_concurrent << "ms\n";

    // 验证结果正确（无论耗时如何，结果必须正确）
    for (long long val : data) {
        EXPECT_EQ(val, 2);
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
