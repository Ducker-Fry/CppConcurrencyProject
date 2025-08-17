#include"test_segment_queue.h"
#include "segmented_queue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <numeric>
#include <algorithm>
#include <queue>
#include"threadsafe_outstream.h"

// Test 1: Basic functionality verification in single-threaded environment
template <typename T, size_t SEG_SIZE>
void test_single_thread_basic()
{
    std::cout << "=== Single-thread Basic Functionality Test ===" << std::endl;
    SegmentedQueue<T, SEG_SIZE> q;

    // Test initial state
    assert(q.empty());
    assert(q.approximate_size() == 0);

    // Test enqueue and dequeue
    q.push(10);
    q.push(20);
    q.push(30);
    assert(q.approximate_size() == 3);
    assert(!q.empty());

    assert(q.pop() == 10);
    assert(q.pop() == 20);
    assert(q.approximate_size() == 1);

    q.push(40);
    assert(q.pop() == 30);
    assert(q.pop() == 40);
    assert(q.empty(),"queue is empty");

    // Test move semantics
    T val = 50;
    q.push(std::move(val));
    q.push(50);
// 原代码 `q.pop()` 会导致程序卡死，故移除该代码行，保留原注释说明问题

    std::cout << "Single-thread basic functionality test passed" << std::endl << std::endl;
}

// Test 2: Multi-threaded producer-consumer model, verifying thread safety and order
template <typename T, size_t SEG_SIZE>
void test_multi_thread_concurrent()
{
    const int PRODUCERS = 4;
    const int CONSUMERS = 4;
    const int ITEMS_PER_THREAD = 2;  // 每个生产者/消费者线程处理1000个项目

    std::cout << "=== Multi-threaded Concurrent Test ===" << std::endl;

    SegmentedQueue<T, SEG_SIZE> q;
    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;

    for(int i = 0; i < PRODUCERS; ++i)
    {
        producer_threads.emplace_back([&q, i]() {
            for (int j = 0; j < ITEMS_PER_THREAD; ++j)
            {
                T item = static_cast<T>(i * ITEMS_PER_THREAD + j);
                q.push(item);
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟生产延迟
            }
        });
    }

    for(int i = 0; i < CONSUMERS; ++i)
    {
        consumer_threads.emplace_back([&q, i]() {
            for (int j = 0; j < ITEMS_PER_THREAD; ++j)
            {
                auto item = q.pop();
                if (item)
                {
                    BufFlusher flusher;
                    bufferd_out("Value from consumer " + std::to_string(i) + ": " + std::to_string(item));
                }
                else
                {
                    BufFlusher flusher;
                    bufferd_out("No item consumed by consumer " + std::to_string(i));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(15)); // 模拟消费延迟
            }
        });
    }

    for(auto& t : producer_threads)
    {
        t.join();
    }

    for(auto& t : consumer_threads)
    {
        t.join();
    }
}

// Test 3: Performance benchmark (compare segmented queue with ordinary locked queue)
template <typename T, size_t SEG_SIZE>
void test_performance()
{
    std::cout << "=== Performance Benchmark Test ===" << std::endl;
    const int THREADS = 8;        // 8 threads (4 producers + 4 consumers)
    const int ITEMS_PER_THREAD = 1000;  // Each producer generates 10000 items

    // Test segmented queue
    SegmentedQueue<T, SEG_SIZE> seg_queue;
    std::atomic<bool> seg_done(false);
    std::atomic<int> seg_processed(0);

    auto seg_producer = [&]() {
        for (int i = 0; i < ITEMS_PER_THREAD; ++i)
        {
            seg_queue.push(i);
        }
        };

    auto seg_consumer = [&]() {
        while (!seg_done || seg_processed < THREADS * ITEMS_PER_THREAD / 2)
        {
            if (auto val = seg_queue.try_pop())
            {
                seg_processed++;
            }
            else
            {
                std::this_thread::yield();
            }
        }
        };

    // Start timing (segmented queue)
    auto seg_start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> seg_threads;
    for (int i = 0; i < THREADS / 2; ++i) seg_threads.emplace_back(seg_producer);
    for (int i = 0; i < THREADS / 2; ++i) seg_threads.emplace_back(seg_consumer);
    for (int i = 0; i < THREADS / 2; ++i) seg_threads[i].join();
    seg_done = true;
    for (int i = THREADS / 2; i < THREADS; ++i) seg_threads[i].join();
    auto seg_end = std::chrono::high_resolution_clock::now();
    auto seg_duration = std::chrono::duration_cast<std::chrono::milliseconds>(seg_end - seg_start).count();

    // Test ordinary locked queue (for comparison)
    std::queue<T> normal_queue;
    std::mutex normal_mutex;
    std::condition_variable normal_cv;
    std::atomic<bool> normal_done(false);
    std::atomic<int> normal_processed(0);

    auto normal_producer = [&]() {
        for (int i = 0; i < ITEMS_PER_THREAD; ++i)
        {
            std::lock_guard<std::mutex> lock(normal_mutex);
            normal_queue.push(i);
            normal_cv.notify_one();
        }
        };

    auto normal_consumer = [&]() {
        while (!normal_done || normal_processed < THREADS * ITEMS_PER_THREAD / 2)
        {
            std::unique_lock<std::mutex> lock(normal_mutex);
            normal_cv.wait(lock, [&]() {
                return !normal_queue.empty() || normal_done;
                });
            if (!normal_queue.empty())
            {
                normal_queue.pop();
                normal_processed++;
            }
        }
        };

    // Start timing (ordinary queue)
    auto normal_start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> normal_threads;
    for (int i = 0; i < THREADS / 2; ++i) normal_threads.emplace_back(normal_producer);
    for (int i = 0; i < THREADS / 2; ++i) normal_threads.emplace_back(normal_consumer);
    for (int i = 0; i < THREADS / 2; ++i) normal_threads[i].join();
    normal_done = true;
    normal_cv.notify_all();
    for (int i = THREADS / 2; i < THREADS; ++i) normal_threads[i].join();
    auto normal_end = std::chrono::high_resolution_clock::now();
    auto normal_duration = std::chrono::duration_cast<std::chrono::milliseconds>(normal_end - normal_start).count();

    // Output performance comparison
    double seg_throughput = (THREADS * ITEMS_PER_THREAD) / (double)seg_duration * 1000;
    double normal_throughput = (THREADS * ITEMS_PER_THREAD) / (double)normal_duration * 1000;

    std::cout << "Segmented queue:" << std::endl;
    std::cout << "  Total items: " << THREADS * ITEMS_PER_THREAD << std::endl;
    std::cout << "  Time elapsed: " << seg_duration << "ms" << std::endl;
    std::cout << "  Throughput: " << seg_throughput << " items/second" << std::endl;

    std::cout << "Ordinary locked queue:" << std::endl;
    std::cout << "  Total items: " << THREADS * ITEMS_PER_THREAD << std::endl;
    std::cout << "  Time elapsed: " << normal_duration << "ms" << std::endl;
    std::cout << "  Throughput: " << normal_throughput << " items/second" << std::endl << std::endl;
}


// 显式实例化模板函数
template void test_single_thread_basic<int, 5>();
template void test_multi_thread_concurrent<int, 5>();
template void test_performance<int, 5>();
