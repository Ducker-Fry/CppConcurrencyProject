#pragma once
#include<iterator>
#include<vector>
#include<exception>
#include<mutex>
#include<algorithm>
#include<queue>

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

template<typename InputIt, typename Func>
void parallel_for_each_d(InputIt first, InputIt last, Func func)
{
    // 并发遍历算法实现，动态分割
    // 计算总元素数量
    const size_t distance = std::distance(first, last);
    if (distance == 0) return;

    // 1. 确定线程数和最小任务粒度
    const size_t min_per_thread = 25; // 每个线程至少处理25个元素
    const size_t num_blocks = std::max<size_t>(1, (distance + min_per_thread - 1) / min_per_thread);
    const size_t block_size = std::max<size_t>(1, distance / num_blocks); 

    // 2. 线程安全的任务队列：存储待处理的[start, end)范围
    std::queue<std::pair<InputIt, InputIt>> task_queue; // 任务队列
    std::mutex queue_mutex; // 互斥锁，用于保护任务队列
    std::condition_variable cv; // 条件变量，用于线程同步
    std::atomic<bool> done{ false }; // 标志，表示任务是否完成
    std::exception_ptr eptr; // 用于存储异常指针

    // 3. 将任务分割成块并添加到队列
    InputIt block_start = first;
    InputIt block_end = first;
    for(int i = 0; i < num_blocks -1;i++)
    {
        std::advance(block_end, block_size);
        task_queue.push({ block_start, block_end });
        block_start = block_end;
    }
    task_queue.push({ block_start, last }); // 添加最后一个块

    // 4. 定制工作线程函数
    auto worker = [&]() {
        while (done)
        {
            std::pair<InputIt, InputIt> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [&]() { return !task_queue.empty() || done.load(); });
                if (done.load() && task_queue.empty())
                {
                    return; // 如果没有任务且已完成，退出线程
                }
                task = task_queue.front();
                task_queue.pop();
            }

            try
            {
                std::for_each(task.first, task.second, func); // 执行任务
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if(!eptr) // 如果没有异常被捕获
                {
                    eptr = std::current_exception(); // 捕获异常
                }

                done.store(true,std::memory_order_release); // 设置完成标志
                cv.notify_all(); // 通知所有线程
                return; // 退出线程
            }
        }
        };

    // 5. 启动工作线程
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const size_t num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, task_queue.size()); // 确保线程数不超过硬件支持的线程数且线程数不超过任务数，避免空转。
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for(int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(worker); // 启动工作线程
    }

    //6. 等待所有线程完成
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [&]() { return done.load() || !task_queue.empty(); });
        done.store(true, std::memory_order_release); // 设置完成标志,通知所有线程可以退出
    }

    cv.notify_all(); // 通知所有线程

    // 7. 等待所有线程结束
    for(auto& thread : threads)
    {
        if(thread.joinable())
        {
            thread.join(); // 等待线程结束
        }
    }

    if(eptr) // 如果有异常被捕获
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

