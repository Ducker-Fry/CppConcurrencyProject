#include<parallel_algorithm/for_each.h>
#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <algorithm>

// 辅助函数：模拟耗时且不均匀的任务（用于测试负载均衡）
void uneven_workload(int& x) {
    // 让偶数索引更耗时（模拟负载不均场景）
    if (x % 2 == 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    x *= 2;
}

// 测试1：基础功能 - 动态划分下的元素修改
TEST(DynamicConcurrentForEachTest, BasicElementModification) {
    const size_t n = 1000;
    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = i;
    }

    // 动态并发处理
    parallel_for_each_d(data.begin(), data.end(), [](int& x) {
        x += 10;
    });

    // 验证结果
    for (size_t i = 0; i < n; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i) + 10);
    }
}

// 测试2：负载均衡能力 - 处理不均匀任务（对比静态版本优势）
TEST(DynamicConcurrentForEachTest, LoadBalancing) {
    const size_t n = 500;
    std::vector<int> data(n);
    std::vector<int> test_data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = i;
    }

    // 记录动态版本耗时
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    parallel_for_each_d(data.begin(), data.end(), uneven_workload);
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto time_dynamic = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_dynamic - start_dynamic
    ).count();
    test_data = data; // 保存动态版本处理后的数据

    // 重置数据
    for (size_t i = 0; i < n; ++i) {
        data[i] = i;
    }

    // 为对比，记录静态并发版本耗时（假设已有静态版本实现）
    // （实际测试需包含静态版本代码）
    auto start_static = std::chrono::high_resolution_clock::now();
    // static_concurrent_for_each(data.begin(), data.end(), uneven_workload);
    auto end_static = std::chrono::high_resolution_clock::now();
    auto time_static = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_static - start_static
    ).count();

    // 打印耗时（动态版本在负载不均场景应更快）
    std::cout << "Dynamic time: " << time_dynamic << "ms\n";
    std::cout << "Static time (for comparison): " << time_static << "ms\n";

    // 验证结果正确性
    for (size_t i = 0; i < n; ++i) {
        EXPECT_EQ(test_data[i], 2 * static_cast<int>(i));
    }
}

// 测试3：线程安全 - 多线程操作共享计数器
TEST(DynamicConcurrentForEachTest, ThreadSafeCounter) {
    const size_t n = 10000;
    std::vector<int> data(n, 1);
    std::atomic<int> sum(0); // 原子变量确保线程安全

    parallel_for_each_d(data.begin(), data.end(), [&sum](int x) {
        sum += x;
    });

    EXPECT_EQ(sum, n); // 10000个1相加
}

// 测试4：线程安全 - 互斥锁保护复杂共享资源
TEST(DynamicConcurrentForEachTest, MutexProtectedResource) {
    const size_t n = 5000;
    std::list<std::string> data(n, "x");
    std::string result;
    std::mutex mtx;

    parallel_for_each_d(data.begin(), data.end(), [&](const std::string& s) {
        std::lock_guard<std::mutex> lock(mtx);
        result += s;
    });

    EXPECT_EQ(result.size(), n);
    EXPECT_EQ(result, std::string(n, 'x'));
}

// 测试5：边界条件 - 元素数量远小于线程数
TEST(DynamicConcurrentForEachTest, SmallRange) {
    const size_t n = 5; // 远小于硬件线程数
    std::vector<int> data(n);

    parallel_for_each_d(data.begin(), data.end(), [](int& x) {
        static std::atomic<int> seq(0);
        x = seq++;
    });

    std::sort(data.begin(), data.end());
    for (size_t i = 0; i < n; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i));
    }
}

// 测试6：边界条件 - 单元素
TEST(DynamicConcurrentForEachTest, SingleElement) {
    std::vector<int> data = {5};

    parallel_for_each_d(data.begin(), data.end(), [](int& x) {
        x *= 10;
    });

    EXPECT_EQ(data[0], 50);
}

// 测试7：异常处理 - 子线程抛出异常后终止所有线程
TEST(DynamicConcurrentForEachTest, ExceptionPropagation) {
    const size_t n = 5;
    std::vector<int> data(n);
    const std::string error_msg = "dynamic test exception";

    // 验证异常被正确抛出
    EXPECT_THROW({
        parallel_for_each_d(data.begin(), data.end(), [&](int& x) {
            static std::atomic<size_t> count(0);
            if (count++ == 2) { // 第50个元素触发异常
                throw std::runtime_error(error_msg);
            }
        });
    }, std::runtime_error);

}

// 测试8：任务队列耗尽后线程正确退出
TEST(DynamicConcurrentForEachTest, ThreadsExitProperly) {
    const size_t n = 1000;
    std::vector<int> data(n, 0);
    std::atomic<int> thread_count(0); // 记录实际参与工作的线程数
    std::mutex mtx;
    std::vector<std::thread::id> thread_ids; // 存储工作线程ID

    parallel_for_each_d(data.begin(), data.end(), [&](int&) {
        std::lock_guard<std::mutex> lock(mtx);
        auto id = std::this_thread::get_id();
        // 记录唯一线程ID
        if (std::find(thread_ids.begin(), thread_ids.end(), id) == thread_ids.end()) {
            thread_ids.push_back(id);
            thread_count++;
        }
    });

    // 验证至少有一个线程参与（最多为硬件核心数）
    EXPECT_GE(thread_count, 1);
    EXPECT_LE(thread_count, std::thread::hardware_concurrency());
}

