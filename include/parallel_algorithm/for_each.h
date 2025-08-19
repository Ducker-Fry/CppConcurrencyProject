#pragma once
#include<iterator>
#include<vector>
#include<exception>
#include<mutex>
#include<algorithm>

//串行遍历算法实现
template<typename First, typename Last, typename Func>
Func my_for_each(First first, Last last, Func func)
{
    for (; first != last; ++first)
    {
        func(*first);
    }
    return func;
}

//并发遍历算法实现，静态分割
template<typename InputIt, typename Func>
void parallel_for_each_s(InputIt first, InputIt last, Func func)
{
    size_t distance = std::distance(first, last);
    if (distance == 0) return;

    // 1. 确定线程数和最小任务粒度
    const size_t min_per_thread = 25; // 每个线程至少处理25个元素
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const size_t max_threads = (distance + min_per_thread - 1) / min_per_thread;

    // 确保线程数不超过硬件支持的线程数
    const size_t num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);

    // 2. 计算每个线程处理的范围
    const size_t block_size = distance / num_threads;
    std::vector<std::thread> threads(num_threads); // 创建线程容器。

    //异常处理
    std::exception_ptr eptr; // 用于存储异常指针
    std::mutex mtx; // 互斥锁，用于保护异常指针

    // 3. 启动线程并分配任务
    InputIt block_start = first;
    for (int i = 0; i < num_threads - 1; ++i)
    {
        InputIt block_end = block_start;
        std::advance(block_end, block_size);
        threads[i] = std::thread([block_start, block_end, &func, &eptr, &mtx]() {
            try
            {
                std::for_each(block_start, block_end, func);
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(mtx);
                eptr = std::current_exception(); // 捕获异常
            }
            });
        block_start = block_end;
    }

    // 处理最后一个块
    InputIt block_end = last;
    try
    {
        std::for_each(block_start, block_end, func);
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(mtx);
        eptr = std::current_exception(); // 捕获异常
    }

    // 4. 等待所有线程完成
    for (auto& thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    //5. 处理异常
    if (eptr)
    {
        try
        {
            std::rethrow_exception(eptr); // 重新抛出捕获的异常
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::string("Parallel for_each failed: ") + e.what());
        }
    }
}
