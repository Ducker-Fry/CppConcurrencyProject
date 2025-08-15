#include"hpq_test.h"

// 测试1：单线程环境下的基本功能验证
void test_single_thread()
{
    std::cout << "=== Single Thread Test Start ===" << std::endl;
    HierarchicalPriorityQueue<int> hpq(3, 2);  // 局部队列阈值3，最多窃取2个元素

    // 测试push和pop的基本功能
    hpq.push(3);
    hpq.push(1);
    hpq.push(2);
    assert(hpq.size() == 3);

    // 验证优先级（默认大顶堆，应按3→2→1弹出）
    assert(hpq.wait_and_pop() == 3);
    assert(hpq.wait_and_pop() == 2);
    assert(hpq.wait_and_pop() == 1);
    assert(hpq.empty());

    // 测试移动语义
    hpq.push(10);
    hpq.push(std::move(20));  // 右值引用版本
    assert(hpq.wait_and_pop() == 20);
    assert(hpq.wait_and_pop() == 10);

    std::cout << "单线程测试通过" << std::endl << std::endl;
}

// 测试2：多线程生产者-消费者模型，验证线程安全和优先级
void test_multi_thread()
{
    std::cout << "=== Multi Threads Test Start ===" << std::endl;
    const int PRODUCERS = 4;    // 4个生产者
    const int CONSUMERS = 2;    // 2个消费者
    const int ITEMS_PER_PRODUCER = 100;  // 每个生产者生成100个元素
    const int TOTAL_ITEMS = PRODUCERS * ITEMS_PER_PRODUCER;

    HierarchicalPriorityQueue<int> hpq(10, 5);  // 局部阈值10，最多窃取5个
    std::atomic<int> items_processed(0);        // 已处理的元素数量
    std::vector<int> results;                   // 存储消费结果
    std::mutex results_mutex;                   // 保护结果向量的锁

    // 生产者线程：生成不同范围的优先级元素
    auto producer = [&](int producer_id) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
        {
            // 不同生产者生成不同优先级范围（确保全局有序性可验证）
            int priority = (producer_id + 1) * 1000 + (ITEMS_PER_PRODUCER - i);
            hpq.push(priority);
            // 模拟工作负载
            std::this_thread::yield();
        }
        };

    // 消费者线程：弹出元素并验证优先级
    auto consumer = [&](int consumer_id) {
        while (items_processed < TOTAL_ITEMS)
        {
            int val = hpq.wait_and_pop();
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(val);
            }
            items_processed++;
            // 模拟工作负载
            std::this_thread::yield();
        }
        };

    // 启动线程
    std::vector<std::thread> producers;
    for (int i = 0; i < PRODUCERS; ++i)
    {
        producers.emplace_back(producer, i);
    }

    std::vector<std::thread> consumers;
    for (int i = 0; i < CONSUMERS; ++i)
    {
        consumers.emplace_back(consumer, i);
    }

    // 等待所有线程完成
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // 验证结果：元素总数正确
    assert(results.size() == TOTAL_ITEMS);

    // 验证优先级：结果应按降序排列（大顶堆）
    bool is_sorted = true;
    for (size_t i = 1; i < results.size(); ++i)
    {
        if (results[i - 1] < results[i])
        {  // 前一个元素应 >= 后一个元素
            is_sorted = false;
            break;
        }
    }
    assert(is_sorted);

    std::cout << "Multi Threads Test Succeed (" << TOTAL_ITEMS << " elements" << std::endl << std::endl;
}

// 测试3：性能基准测试（测量吞吐量）
void test_performance()
{
    std::cout << "=== 性能基准测试 ===" << std::endl;
    const int THREADS = 8;       // 8个线程（4生产者+4消费者）
    const int ITEMS_PER_THREAD = 10000;  // 每个生产者生成10000个元素

    HierarchicalPriorityQueue<int> hpq(50, 10);
    std::atomic<bool> done(false);
    std::atomic<int> items_processed(0);

    // 生产者：推送递增的优先级（测试插入性能）
    auto producer = [&]() {
        for (int i = 0; i < ITEMS_PER_THREAD; ++i)
        {
            hpq.push(i);  // 优先级从0到9999
        }
        };

    // 消费者：弹出所有元素
    auto consumer = [&]() {
        while (!done || items_processed < THREADS * ITEMS_PER_THREAD / 2)
        {
            if (auto val = hpq.try_pop())
            {
                items_processed++;
            }
            else
            {
                std::this_thread::yield();  // 队列为空时让出CPU
            }
        }
        };

    // 计时开始
    auto start = std::chrono::high_resolution_clock::now();

    // 启动线程（一半生产者，一半消费者）
    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS / 2; ++i) threads.emplace_back(producer);
    for (int i = 0; i < THREADS / 2; ++i) threads.emplace_back(consumer);

    // 等待生产者完成
    for (int i = 0; i < THREADS / 2; ++i) threads[i].join();
    done = true;  // 通知消费者可以结束

    // 等待消费者完成
    for (int i = THREADS / 2; i < THREADS; ++i) threads[i].join();

    // 计时结束
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double throughput = (THREADS * ITEMS_PER_THREAD) / (double)duration * 1000;  // 每秒处理的元素数

    std::cout << "The Result of Performance Test : " << std::endl;
    std::cout << "Total Elements :" << THREADS * ITEMS_PER_THREAD << std::endl;
    std::cout << "Time :" << duration << "ms" << std::endl;
    std::cout << "handling capacity: " << throughput << "Element/s" << std::endl << std::endl;
}

void test_all()
{
    try
    {
        test_single_thread();   // 验证基本功能
        test_multi_thread();    // 验证多线程安全和优先级语义
        test_performance();     // 测量性能指标
        std::cout << "All tests succeed !" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Tests Fail !" << e.what() << std::endl;
    }
}
